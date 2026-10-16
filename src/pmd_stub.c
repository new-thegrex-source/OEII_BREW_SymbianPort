/* Stand-in for src/pmd.c on the phone. The desktop decoder shells out to
 * libavcodec's QCELP-13k decoder, which does not exist in the S60 SDK.
 * Returning failure makes the engine skip that clip instead of failing the
 * link. Effects that are already PCM still go through Audio_play. */

#include "pmd.h"
#include <stddef.h>
#include <stdint.h>

int Pmd_decode(const uint8_t *cmf, size_t len, int16_t **pcm, int *samples)
{
	(void)cmf;
	(void)len;
	if (pcm)
		*pcm = 0;
	if (samples)
		*samples = 0;
	return -1;
}
