# CMakeLists for qrfast Android NDK build
cmake_minimum_required(VERSION 3.16)
project(qrfast_jni C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# BULLETPROOF: Find the exact absolute path of the root directory without using relative dots
get_filename_component(ANDROID_APP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
get_filename_component(QRFAST_ROOT "${ANDROID_APP_DIR}/../../.." ABSOLUTE)

set(QUIRC_DIR "${QRFAST_ROOT}/third_party/quirc")

# Compile quirc with single-precision float math optimized for mobile ARM
add_library(quirc STATIC
        "${QUIRC_DIR}/lib/quirc.c"
        "${QUIRC_DIR}/lib/identify.c"
        "${QUIRC_DIR}/lib/decode.c"
        "${QUIRC_DIR}/lib/version_db.c"
)
target_include_directories(quirc PUBLIC "${QUIRC_DIR}/lib")
target_compile_definitions(quirc PRIVATE QUIRC_FLOAT_TYPE=float QUIRC_USE_TGMATH)

# Compile qrfast core
add_library(qrfast STATIC
        "${QRFAST_ROOT}/src/qrfast.c"
)
target_include_directories(qrfast PUBLIC "${QRFAST_ROOT}/include")
target_link_libraries(qrfast PUBLIC quirc m)

# JNI bridge shared library for Android
add_library(qrfast_jni SHARED
        qrfast_jni.cpp
)
target_include_directories(qrfast_jni PRIVATE "${QRFAST_ROOT}/include")
target_link_libraries(qrfast_jni PRIVATE
        qrfast
        android
        log
)
