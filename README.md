# QRFastPay Engine ⚡

A high-performance, ultra-lightweight native Android QR code scanning engine optimized for instant mobile payment routing. This application sidesteps modern bloatware architectures by combining a bare-metal C core with hardware-level ARM NEON vector register acceleration to minimize end-to-end transaction processing latency.

---

## 📐 Core Architecture & Algorithmic Math

Instead of passing heavy image data through high-level Java or Kotlin abstraction layers (which introduces performance penalties and stutters from Android’s Garbage Collector), QRFastPay drops raw grayscale camera pixel buffers directly down to a native C pipeline via the **JNI (Java Native Interface)** bridge.

### 1. Vectorized Luminance Transformation (Color to Grayscale)
The engine processes incoming YUV/RGB camera streams by flattening frames into linear pixel arrays. To convert standard RGB frames to grayscale efficiently, the pipeline implements standard colorimetric luminance weighting vectors. The low-level mathematical operation for each pixel is defined as:

\[Y = 0.299R + 0.587G + 0.114B\]

In the native core, these floating-point operations are converted to high-performance integer math by scaling the weights by 256 (77 ≈ 256 × 0.299, 150 ≈ 256 × 0.587, 29 ≈ 256 × 0.114). The values are processed across 64-bit and 128-bit vector lanes simultaneously, ensuring zero-heap runtime memory allocation.

### 2. O(Width) Rolling Integral Adaptive Binarization
To handle diverse real-world lighting conditions at merchant checkouts, the engine implements a custom C version of **Bradley Local-Mean Adaptive Thresholding**. 

Standard adaptive binarization algorithms calculate a full 2D Integral Image (Sat), requiring O(Width × Height) storage allocations. QRFastPay eliminates this memory overhead entirely by maintaining a rolling column-sum buffer. It calculates a moving average of a spatial window S × S around each pixel, binarizing the image on-the-fly:

\[T(x,y) = \frac{1}{S^2} \sum_{i=-S/2}^{S/2} \sum_{j=-S/2}^{S/2} I(x+i, y+j)\]

\[B(x,y) = \begin{cases} 0 & \text{if } I(x,y) < T(x,y) \times \left(1 - \frac{t}{100}\right) \\ 255 & \text{otherwise} \end{cases}\]

This optimization allows the entire thresholding step to pass through the image with a strict spatial complexity bounded strictly by the width of a single row.

---

## 🛠️ Native NDK Toolchain & Technical Fixes

During the system engineering and compilation setup, multiple cross-platform architecture blockers were identified and systematically resolved:

### 1. Robust CMake Cross-Compilation Path Resolution
Standard Android NDK build systems evaluate relative directory structures blindly from deep generated internal workspace directories (such as `.cxx/Debug/2l62296u/arm64-v8a`). To prevent relative folder out-of-bounds compilation failures on Windows host systems, absolute path resolution targets are anchored dynamically using `get_filename_component`:

```cmake
# Dynamic absolute directory root binding to bypass host cache deep-linking
get_filename_component(ANDROID_APP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
get_filename_component(QRFAST_ROOT "${ANDROID_APP_DIR}/../../.." ABSOLUTE)

set(QUIRC_DIR "${QRFAST_ROOT}/third_party/quirc")
```

### 2. ARM NEON Intrinsic Vector Realignment
The core file `qrfast.c` accelerates binarization using SIMD (Single Instruction, Multiple Data) processing loops. Because the ARM hardware profile lacks a widening scalar-multiplication variant (`_n_`) for 8-bit unsigned matrices, channel weights are duplicated across vector registers using `vdup_n_u8` before executing long widening operations (`vmull_u8` / `vmlal_u8`):

```c
#if defined(__ARM_NEON) || defined(__aarch64__)
#include <arm_neon.h>

// Duplicate scalar weights into vector lanes for R, G, B channels
uint8x8_t weight_r = vdup_n_u8(77u);
uint8x8_t weight_g = vdup_n_u8(150u);
uint8x8_t weight_b = vdup_n_u8(29u);

// Widening SIMD processing block targeting 8 pixels simultaneously
uint16x8_t sum = vmull_u8(px.val[0], weight_r);
sum = vmlal_u8(sum, px.val[1], weight_g);
sum = vmlal_u8(sum, px.val[2], weight_b);
#endif
```

Additionally, architecture inclusions are sandboxed inside preprocessor macro checks (`#if defined(__ARM_NEON)`) to preserve clean multi-platform cross-compilation across x86 and 64-bit target platforms.

---

## 📂 Project Directory Structure

```text
qrfast/
├── android/                   # Android Studio Native Application Module
│   ├── app/
│   │   ├── src/
│   │   │   └── main/
│   │   │       ├── cpp/       # JNI Bridge Architecture
│   │   │       │   ├── CMakeLists.txt  # Core Native Build Script
│   │   │       │   └── qrfast_jni.cpp  # C++ Native Interface Methods
│   │   │       ├── java/      # Kotlin App Navigation & Intent Layouts
│   │   │       └── AndroidManifest.xml
│   │   └── build.gradle.kts
│   ├── build.gradle.kts
│   └── settings.gradle.kts
├── include/                   # Shared Engine Headers
│   └── qrfast.h
├── src/                       # Core Processing Engine
│   └── qrfast.c               # NEON-Optimized Algorithm Code
├── third_party/
│   └── quirc/                 # Low-level Matrix Decoder Library Submodule
└── README.md
```

---

## 🔗 Native Intent Payment Routing

Once the underlying `quirc` engine reads alignment grids, straightens perspective distortion, and decodes the string data matrix, it extracts a standard raw `upi://pay?...` URI scheme string. 

The Android application instantly intercepts this string and constructs a native Android intent flag:

```kotlin
val intent = Intent(Intent.ACTION_VIEW, Uri.parse(upiString))
val chooser = Intent.createChooser(intent, "Pay securely via:")
context.startActivity(chooser)
```

This bypasses payment aggregators or middleman code architectures, prompting the device OS to natively open a bottom sheet presenting direct routing selectors like **Kotak811, BHIM, INDmoney, or Navi** for immediate transaction finalization.

---

## 📦 Local Deployment Blueprint

### Prerequisites
* **Android Studio** (Ladybug/Quail or later)
* **Android NDK** (Version 28.2.13676358 or later)
* **Physical Target Device** (e.g., Samsung Galaxy M31s) configured with **USB Debugging** active under Developer Options.

### Compilation Pipeline Execution
Open your terminal inside the application environment folder, wipe out any generated cross-compilation workspace memories, and push the optimized package to the device:

```bash
# Navigate to the Android module
cd android

# Wipe compilation cache structures
./gradlew.bat clean

# Compile native binary assets and flash to phone over USB debugging
./gradlew.bat installDebug --no-configuration-cache
```
