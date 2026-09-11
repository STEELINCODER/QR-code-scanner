package com.example.qrfastpay

import android.Manifest
import android.content.pm.PackageManager
import android.graphics.Color
import android.os.Bundle
import android.view.ViewGroup
import android.widget.LinearLayout
import android.widget.TextView
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.camera.core.CameraSelector
import androidx.camera.core.ImageAnalysis
import androidx.camera.core.Preview
import androidx.camera.lifecycle.ProcessCameraProvider
import androidx.camera.view.PreviewView
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color as ComposeColor
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.core.content.ContextCompat
import com.example.qrfastpay.theme.QRFastPayTheme
import java.util.concurrent.Executors

class MainActivity : ComponentActivity() {

    private var hasCameraPermission by mutableStateOf(false)
    private var detectedPayment by mutableStateOf<PaymentDetails?>(null)
    private var scanStats by mutableStateOf("Ready to scan")
    private val cameraExecutor = Executors.newSingleThreadExecutor()

    private val requestPermissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestPermission()
    ) { isGranted ->
        hasCameraPermission = isGranted
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()

        hasCameraPermission = ContextCompat.checkSelfPermission(
            this, Manifest.permission.CAMERA
        ) == PackageManager.PERMISSION_GRANTED

        if (!hasCameraPermission) {
            requestPermissionLauncher.launch(Manifest.permission.CAMERA)
        }

        setContent {
            QRFastPayTheme {
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colorScheme.background
                ) {
                    if (hasCameraPermission) {
                        ScannerScreen(
                            detectedPayment = detectedPayment,
                            scanStats = scanStats,
                            onScanFrame = { buffer, rowStride, width, height ->
                                QrScanner.nativeInit(width, height)
                                val startTime = System.nanoTime()
                                val payload = QrScanner.nativeScanYPlane(buffer, rowStride)
                                val elapsedMs = (System.nanoTime() - startTime) / 1_000_000.0

                                if (payload != null && detectedPayment == null) {
                                    val payment = PaymentHandler.parse(payload)
                                    runOnUiThread {
                                        scanStats = "Decoded in ${"%.1f".format(elapsedMs)} ms"
                                        detectedPayment = payment
                                        if (payment != null) {
                                            // Launch payment chooser immediately
                                            PaymentHandler.launchUpiIntent(this, payment)
                                        }
                                    }
                                }
                            },
                            onReset = {
                                detectedPayment = null
                                scanStats = "Scanning..."
                            }
                        )
                    } else {
                        PermissionRequiredScreen {
                            requestPermissionLauncher.launch(Manifest.permission.CAMERA)
                        }
                    }
                }
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        cameraExecutor.shutdown()
        QrScanner.nativeDestroy()
    }
}

@Composable
fun ScannerScreen(
    detectedPayment: PaymentDetails?,
    scanStats: String,
    onScanFrame: (java.nio.ByteBuffer, Int, Int, Int) -> Unit,
    onReset: () -> Unit
) {
    val context = LocalContext.current
    val lifecycleOwner = androidx.lifecycle.compose.LocalLifecycleOwner.current

    Box(modifier = Modifier.fillMaxSize()) {
        // Live Camera Preview
        AndroidView(
            factory = { ctx ->
                val previewView = PreviewView(ctx).apply {
                    layoutParams = ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT,
                        ViewGroup.LayoutParams.MATCH_PARENT
                    )
                }

                val cameraProviderFuture = ProcessCameraProvider.getInstance(ctx)
                cameraProviderFuture.addListener({
                    val cameraProvider = cameraProviderFuture.get()
                    val preview = Preview.Builder().build().also {
                        it.setSurfaceProvider(previewView.surfaceProvider)
                    }

                    val imageAnalysis = ImageAnalysis.Builder()
                        .setTargetResolution(android.util.Size(640, 640))
                        .setBackpressureStrategy(ImageAnalysis.STRATEGY_KEEP_ONLY_LATEST)
                        .setOutputImageFormat(ImageAnalysis.OUTPUT_IMAGE_FORMAT_YUV_420_888)
                        .build()

                    imageAnalysis.setAnalyzer(Executors.newSingleThreadExecutor()) { imageProxy ->
                        val plane = imageProxy.planes[0]
                        onScanFrame(
                            plane.buffer,
                            plane.rowStride,
                            imageProxy.width,
                            imageProxy.height
                        )
                        imageProxy.close()
                    }

                    try {
                        cameraProvider.unbindAll()
                        cameraProvider.bindToLifecycle(
                            lifecycleOwner,
                            CameraSelector.DEFAULT_BACK_CAMERA,
                            preview,
                            imageAnalysis
                        )
                    } catch (e: Exception) {
                        e.printStackTrace()
                    }
                }, ContextCompat.getMainExecutor(ctx))

                previewView
            },
            modifier = Modifier.fillMaxSize()
        )

        // Viewfinder Target Overlay
        Box(
            modifier = Modifier
                .size(260.dp)
                .align(Alignment.Center)
                .border(2.5.dp, ComposeColor.Cyan, RoundedCornerShape(16.dp))
        )

        // Top Status Bar
        Surface(
            color = ComposeColor.Black.copy(alpha = 0.7f),
            shape = RoundedCornerShape(12.dp),
            modifier = Modifier
                .align(Alignment.TopCenter)
                .statusBarsPadding()
                .padding(top = 16.dp)
        ) {
            Text(
                text = "⚡ qrfast: $scanStats",
                color = ComposeColor.White,
                fontSize = 14.sp,
                fontWeight = FontWeight.SemiBold,
                modifier = Modifier.padding(horizontal = 16.dp, vertical = 8.dp)
            )
        }

        // Bottom Payment Action Card
        if (detectedPayment != null) {
            Surface(
                color = MaterialTheme.colorScheme.surface,
                shadowElevation = 8.dp,
                shape = RoundedCornerShape(topStart = 20.dp, topEnd = 20.dp),
                modifier = Modifier
                    .align(Alignment.BottomCenter)
                    .fillMaxWidth()
            ) {
                Column(
                    modifier = Modifier
                        .navigationBarsPadding()
                        .padding(20.dp)
                ) {
                    Text(
                        text = "Payee: ${detectedPayment.payeeName}",
                        fontSize = 18.sp,
                        fontWeight = FontWeight.Bold
                    )
                    Text(
                        text = "VPA: ${detectedPayment.payeeVpa}",
                        fontSize = 14.sp,
                        color = ComposeColor.Gray
                    )
                    if (detectedPayment.amount != null) {
                        Text(
                            text = "Amount: ₹${detectedPayment.amount}",
                            fontSize = 22.sp,
                            fontWeight = FontWeight.ExtraBold,
                            color = MaterialTheme.colorScheme.primary,
                            modifier = Modifier.padding(vertical = 4.dp)
                        )
                    }
                    Spacer(modifier = Modifier.height(12.dp))
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        Button(
                            onClick = { PaymentHandler.launchUpiIntent(context, detectedPayment) },
                            modifier = Modifier.weight(1f)
                        ) {
                            Text("Pay Now")
                        }
                        OutlinedButton(onClick = onReset) {
                            Text("Scan Again")
                        }
                    }
                }
            }
        }
    }
}

@Composable
fun PermissionRequiredScreen(onRequestPermission: () -> Unit) {
    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(32.dp),
        verticalArrangement = Arrangement.Center,
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Text(
            text = "Camera Permission Needed",
            fontSize = 20.sp,
            fontWeight = FontWeight.Bold,
            textAlign = TextAlign.Center
        )
        Spacer(modifier = Modifier.height(12.dp))
        Text(
            text = "Camera access is required to scan payment QR codes with sub-10ms latency.",
            fontSize = 14.sp,
            textAlign = TextAlign.Center
        )
        Spacer(modifier = Modifier.height(24.dp))
        Button(onClick = onRequestPermission) {
            Text("Grant Permission")
        }
    }
}
