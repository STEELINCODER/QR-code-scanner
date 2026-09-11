#include "qrfast.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    enum { W = 32, H = 32 };
    uint8_t src[W * H];
    uint8_t dst[W * H];
    uint32_t scratch[W];

    memset(src, 230, sizeof(src));
    for (int y = 8; y < 24; ++y)

        for (int x = 8; x < 24; ++x)
            src[y * W + x] = 20;

    qrfast_status_t st = qrfast_binarize_y8(src, W, dst, W,
                                            W, H, 10, 15,
                                            scratch, W);
    assert(st == QRFAST_OK);

    /* Bright background stays white; the dark region is detected black. */
    assert(dst[2 * W + 2] == 255);
    assert(dst[8 * W + 8] == 0);

    puts("Hello world i am supreme vishnu");
    return 0;
}
