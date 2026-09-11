cmake_minimum_required(VERSION 3.16)
project(qrfast C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(QUIRC_DIR "" CACHE PATH "Path to a checkout of https://github.com/dlbeer/quirc")
if(NOT QUIRC_DIR)
    message(FATAL_ERROR "Set QUIRC_DIR to a quirc checkout, e.g. -DQUIRC_DIR=/path/to/quirc")
endif()

add_library(quirc STATIC
    ${QUIRC_DIR}/lib/quirc.c
    ${QUIRC_DIR}/lib/identify.c
    ${QUIRC_DIR}/lib/decode.c
    ${QUIRC_DIR}/lib/version_db.c
)
target_include_directories(quirc PUBLIC ${QUIRC_DIR}/lib)
# Mobile ARM cores generally benefit from single precision inside the
# perspective math, and quirc explicitly supports this compile-time option.
target_compile_definitions(quirc PRIVATE QUIRC_FLOAT_TYPE=float QUIRC_USE_TGMATH)

add_library(qrfast STATIC src/qrfast.c)
target_include_directories(qrfast PUBLIC include)
target_link_libraries(qrfast PUBLIC quirc m)

if(MSVC)
    target_compile_options(qrfast PRIVATE /O2 /W4)
else()
    target_compile_options(qrfast PRIVATE -O3 -Wall -Wextra -Wpedantic)
endif()

option(QRFAST_BUILD_EXAMPLES "Build qrfast examples" ON)
if(QRFAST_BUILD_EXAMPLES)
    add_executable(scan_pgm examples/scan_pgm.c)
    target_link_libraries(scan_pgm PRIVATE qrfast)
endif()
