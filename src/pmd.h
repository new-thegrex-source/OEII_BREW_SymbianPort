#ifndef OE2_PMD_H
#define OE2_PMD_H

#include <stddef.h>
#include <stdint.h>

#define PMD_SAMPLE_RATE 8000

/* Decode a Qualcomm CMF (.pmd) resource whose track holds QCELP-13k wave data
 * into mono 16-bit PCM at 8 kHz. Returns 0 on success; *pcm is malloc'd. */
int Pmd_decode(const uint8_t *cmf, size_t len, int16_t **pcm, int *samples);

#endif
