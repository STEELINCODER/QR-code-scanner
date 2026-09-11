# qrfast

A deliberately small QR scanning core for payment-app style camera pipelines.
The hot path is designed around fixed frame dimensions, caller-owned scratch
memory, and no per-frame heap allocation.

## Pipeline

1. Consume the camera's Y/luma plane directly.
2. Fast path: feed raw luma to a persistent `quirc` decoder and let its global
   threshold handle ordinary frames.
3. On a miss, run Bradley-style local mean adaptive binarization using an
   O(width) rolling column-sum buffer instead of a full integral image.
4. `quirc` performs finder detection, alignment/perspective geometry, grid
   sampling, mask removal, format/version decoding, and Reed-Solomon recovery.
5. Retry mirrored QR data when decoding fails.
6. After an adaptive miss, retry the binary image inverted for light-on-dark QR codes.

## Allocation model

`qrfast_init()` calls `quirc_new()` and `quirc_resize()`. Those are startup-time
allocations. After initialization, `qrfast_scan_y8()` does not allocate.

For a stricter "no heap ever" target, keep `qrfast_binarize_y8()` unchanged and
replace the quirc backend with an in-place/static-memory fork. On Android/iOS,
the OS camera framework itself will still own buffers, so the practically useful
guarantee is **no allocations in your frame-processing hot path**.

## Get quirc

Clone quirc separately:

    git clone https://github.com/dlbeer/quirc.git third_party/quirc

Then build:

    cmake -S . -B build -DQUIRC_DIR=$PWD/third_party/quirc
    cmake --build build -j

Test a grayscale PGM:

    ./build/scan_pgm frame.pgm

## Camera integration

Prefer native luma formats. Do not convert a full camera frame from RGB if the
platform can expose YUV directly.

- Android: `YUV_420_888`, plane 0 is Y/luma. Pass its row stride to
  `qrfast_scan_y8()`.
- iOS: request a bi-planar 420 pixel buffer and lock the pixel buffer; plane 0
  is luma. Pass its base address and bytes-per-row.

Crop to a viewfinder ROI before scanning when possible. A 640x640 ROI usually
beats brute-forcing a multi-megapixel frame by a large margin.

## Suggested production defaults

- Fixed processing ROI: 480-720 pixels wide.
- Adaptive window radius: width/16, bounded to roughly 8..96 pixels.
- Bradley threshold: 15%.
- Keep the raw/global fast path enabled; adaptive processing is a fallback.
- One scanner instance per camera stream.
- One frame in flight at a time. Drop stale frames rather than queueing them.
- Scan normal polarity first; only run inverted retry after a miss.
- Parse/validate payment payloads only after QR decoding. The QR core must never
  initiate a financial transaction by itself.

## Security boundary

Treat decoded bytes as hostile input. For a payment app, whitelist the accepted
payment scheme, strictly bound all fields, reject duplicate/conflicting critical
parameters, display the final payee and amount to the user, and require explicit
user authorization before handing off to the payment rail.
