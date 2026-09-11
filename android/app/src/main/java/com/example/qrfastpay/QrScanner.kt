package com.example.qrfastpay

import java.nio.ByteBuffer

object QrScanner {
    init {
        System.loadLibrary("qrfast_jni")
    }

    external fun nativeInit(width: Int, height: Int): Boolean
    external fun nativeScanYPlane(byteBuffer: ByteBuffer, rowStride: Int): String?
    external fun nativeDestroy()
}
