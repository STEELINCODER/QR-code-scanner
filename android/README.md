# ⚡ QRFastPay - High-Performance Android Payment QR Scanner

An Android application demonstrating sub-10ms QR scanning designed for high-throughput mobile payment flows (UPI / BharatPe / EMVCo).

---

## 🏗️ Architecture

1. **Zero-Copy Hot Path**:
   - CameraX captures frames using `ImageAnalysis` configured with `OUTPUT_IMAGE_FORMAT_YUV_420_888`.
   - **Plane 0 (Y / Luma / Grayscale)** is handed directly to native C++ via direct memory pointers (`GetDirectBufferAddress`).
   - Zero Bitmap conversions and zero GC allocations per frame.

2. **Native C Core (`qrfast` + `quirc`)**:
   - Direct JNI bindings in `app/src/main/cpp/qrfast_jni.cpp`.
   - Single precision ARM floating point perspective geometry (`QUIRC_FLOAT_TYPE=float`).
   - Bradley local-mean adaptive thresholding fallback for challenging lighting and glare.

3. **Instant Payment Dispatch**:
   - `PaymentHandler.kt` parses standard `upi://pay` URIs.
   - Automatically surfaces payee name, VPA, and pre-filled amount.
   - Dispatches Android's system UPI App Chooser to hand off directly to Google Pay, PhonePe, Paytm, or banking apps.

---

## 🚀 How to Run & Deploy

### Option 1: Android Studio (Recommended)
1. Open **Android Studio**.
2. Select **Open** and select the [`android`](.) folder.
3. Android Studio will automatically sync Gradle, download the NDK if needed, and configure CMake.
4. Plug in your Android phone (with USB debugging enabled) or start an emulator.
5. Click the green **Run ▶** button.

### Option 2: Command Line (Gradle)
From this `android` folder:
```powershell
# Build Debug APK
.\gradlew.bat assembleDebug

# Install on connected device via adb
adb install app/build/outputs/apk/debug/app-debug.apk
```
