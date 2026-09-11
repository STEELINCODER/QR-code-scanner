#include <jni.h>
#include <string>
#include <vector>
#include <android/log.h>
#include "qrfast.h"

#define TAG "qrfast_jni"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

static qrfast_scanner_t g_scanner;
static std::vector<uint32_t> g_scratch;
static int g_current_width = 0;
static int g_current_height = 0;
static bool g_initialized = false;

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_qrfastpay_QrScanner_nativeInit(
    JNIEnv* /* env */, jobject /* this */,
    jint width, jint height) {

    if (g_initialized) {
        if (g_current_width == width && g_current_height == height) {
            return JNI_TRUE;
        }
        qrfast_deinit(&g_scanner);
        g_initialized = false;
    }

    g_scratch.resize(static_cast<size_t>(width));
    qrfast_status_t status = qrfast_init(
        &g_scanner,
        static_cast<int>(width),
        static_cast<int>(height),
        g_scratch.data(),
        static_cast<size_t>(width)
    );

    if (status != QRFAST_OK) {
        LOGE("qrfast_init failed: %s", qrfast_status_string(status));
        return JNI_FALSE;
    }

    g_current_width = width;
    g_current_height = height;
    g_initialized = true;
    LOGI("qrfast initialized for %dx%d frame", width, height);
    return JNI_TRUE;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_qrfastpay_QrScanner_nativeScanYPlane(
    JNIEnv* env, jobject /* this */,
    jobject byteBuffer, jint rowStride) {

    if (!g_initialized) {
        return nullptr;
    }

    auto* pixels = static_cast<uint8_t*>(env->GetDirectBufferAddress(byteBuffer));
    if (!pixels) {
        return nullptr;
    }

    uint8_t payload[2048];
    qrfast_result_t result;

    qrfast_status_t status = qrfast_scan_y8(
        &g_scanner,
        pixels,
        static_cast<size_t>(rowStride),
        payload,
        sizeof(payload),
        &result
    );

    if (status == QRFAST_OK && result.payload_len > 0) {
        std::string text(reinterpret_cast<char*>(payload), result.payload_len);
        return env->NewStringUTF(text.c_str());
    }

    return nullptr;
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_qrfastpay_QrScanner_nativeDestroy(
    JNIEnv* /* env */, jobject /* this */) {

    if (g_initialized) {
        qrfast_deinit(&g_scanner);
        g_scratch.clear();
        g_initialized = false;
        g_current_width = 0;
        g_current_height = 0;
        LOGI("qrfast destroyed");
    }
}
