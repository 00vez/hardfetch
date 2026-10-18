#include "edid.h"

#include <stdio.h>
#include <string.h>

/* Detailed timing descriptors start at 54 and repeat every 18 bytes. */
#define EDID_FIRST_DTD 54
#define EDID_DTD_SIZE  18

static unsigned int be16(const unsigned char* p)
{
    return ((unsigned int)p[0] << 8) | p[1];
}

int edid_is_valid(const unsigned char* e, size_t len)
{
    static const unsigned char header[8] = { 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0 };
    if (!e || len < EDID_BLOCK_SIZE) return 0;
    return memcmp(e, header, 8) == 0;
}

void edid_name(const unsigned char* e, char* out, size_t outLen)
{
    unsigned int i;

    if (outLen == 0) return;
    out[0] = 0;

    for (i = EDID_FIRST_DTD; i + EDID_DTD_SIZE <= EDID_BLOCK_SIZE; i += EDID_DTD_SIZE) {
        if (e[i] != 0x00) continue;           /* pixel clock non-zero: a real timing */
        if (e[i + 3] != 0xFC) continue;       /* not the model name descriptor */
        {
            size_t n = 0;
            while (n < 13 && e[i + 5 + n] != 0x0A && e[i + 5 + n] != 0x00) {
                if (n + 1 < outLen) out[n] = (char)e[i + 5 + n];
                n++;
            }
            while (n > 0 && out[n - 1] == ' ') n--;
            out[n < outLen ? n : outLen - 1] = 0;
            if (out[0]) return;
        }
    }

    /* No name string: fall back to vendor and model number, as E-EDID does. */
    {
        char vendor[4];
        vendor[0] = (char)(((unsigned int)(e[8] >> 2) & 0x1F) + 'A' - 1);
        vendor[1] = (char)(((((unsigned int)e[8] & 0x03) << 3) |
                            (((unsigned int)e[9] & 0xE0) >> 5)) + 'A' - 1);
        vendor[2] = (char)(((unsigned int)e[9] & 0x1F) + 'A' - 1);
        vendor[3] = 0;
        snprintf(out, outLen, "%s-%04X", vendor, be16(e + 10));
    }
}

void edid_physical_size(const unsigned char* e, unsigned int* wMm, unsigned int* hMm)
{
    unsigned int dtdW = (((unsigned int)(e[68] & 0xF0) << 4) + e[66]);
    unsigned int dtdH = (((unsigned int)(e[68] & 0x0F) << 8) + e[67]);
    unsigned int bdpW = (unsigned int)e[21] * 10;
    unsigned int bdpH = (unsigned int)e[22] * 10;
    int dw = (int)dtdW - (int)bdpW;
    int dh = (int)dtdH - (int)bdpH;

    /* The detailed timing descriptor is authoritative when the two agree. */
    if (bdpW > 0 && bdpH > 0 && (dw < 0 ? -dw : dw) < 10 && (dh < 0 ? -dh : dh) < 10) {
        *wMm = dtdW;
        *hMm = dtdH;
    } else {
        *wMm = bdpW;
        *hMm = bdpH;
    }
}
