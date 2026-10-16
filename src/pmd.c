/*
 * Qualcomm CMF ("cmid") wave tracks -> PCM.
 *
 * Every O&E II sound and music resource is a CMF file whose single track carries
 * QCELP-13k audio in 0xF1 "wave" events (draft-atarius-cmf-00). Each event is
 *   FF F1 <u16 len> <u16 codec=0x0044> <u32 total> <u8 seq> <len-7 bytes of QCELP packets>
 * Packets are variable rate: a rate byte, then 0/3/7/16/34 payload bytes.
 * libavcodec's QCELP decoder turns them into 160-sample frames at 8 kHz.
 */
#include "pmd.h"

#include <stdlib.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavutil/log.h>

static unsigned be16(const uint8_t *p) { return (p[0] << 8) | p[1]; }
static uint32_t be32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; }

static int payload_len(unsigned rate)
{
	switch (rate) {
	case 0: case 0x0e: return 0;   /* blank, erasure */
	case 1: return 3;              /* eighth */
	case 2: case 5: return 7;      /* quarter */
	case 3: return 16;             /* half */
	case 4: return 34;             /* full */
	default: return -1;
	}
}

typedef struct {
	int16_t *s;
	int n, cap;
} Pcm;

static int pcm_frame(Pcm *o, const AVFrame *fr)
{
	const float *src = (const float *)fr->data[0];
	if (o->n + fr->nb_samples > o->cap) {
		int cap = o->cap ? o->cap * 2 : 8192;
		int16_t *ns;
		while (cap < o->n + fr->nb_samples)
			cap *= 2;
		ns = realloc(o->s, (size_t)cap * sizeof(int16_t));
		if (!ns)
			return -1;
		o->s = ns;
		o->cap = cap;
	}
	for (int i = 0; i < fr->nb_samples; i++) {
		float v = src[i];
		v = v > 1.f ? 1.f : v < -1.f ? -1.f : v;
		o->s[o->n++] = (int16_t)(v * 32767.f);
	}
	return 0;
}

static int drain(AVCodecContext *ctx, AVFrame *fr, Pcm *o)
{
	for (;;) {
		int r = avcodec_receive_frame(ctx, fr);
		if (r == AVERROR(EAGAIN) || r == AVERROR_EOF)
			return 0;
		if (r < 0 || fr->format != AV_SAMPLE_FMT_FLT || pcm_frame(o, fr) != 0)
			return -1;
		av_frame_unref(fr);
	}
}

int Pmd_decode(const uint8_t *cmf, size_t len, int16_t **pcm, int *samples)
{
	const AVCodec *codec;
	AVCodecContext *ctx = NULL;
	AVPacket *pkt = NULL;
	AVFrame *fr = NULL;
	Pcm out = { 0 };
	uint8_t padded[64 + AV_INPUT_BUFFER_PADDING_SIZE];
	size_t p, end, i;
	int ok = -1;

	*pcm = NULL;
	*samples = 0;
	if (len < 16 || memcmp(cmf, "cmid", 4) != 0)
		return -1;
	for (p = 8; p + 8 <= len && memcmp(cmf + p, "trac", 4) != 0; p++)
		;
	if (p + 8 > len)
		return -1;
	end = p + 8 + be32(cmf + p + 4);
	if (end > len)
		end = len;
	p += 8;

	av_log_set_level(AV_LOG_ERROR);
	codec = avcodec_find_decoder(AV_CODEC_ID_QCELP);
	if (!codec)
		return -1;
	ctx = avcodec_alloc_context3(codec);
	pkt = av_packet_alloc();
	fr = av_frame_alloc();
	if (!ctx || !pkt || !fr || avcodec_open2(ctx, codec, NULL) < 0)
		goto done;

	/* FF E8 vv: wave-channel volume (channel in bits 7-6, 6-bit level), set per resource by the authors (52..63
	 * in this game; a few files have none). The phone's CMF player applied it; we scale linearly since its
	 * exact curve is unknown. */
	int waveVolume = 63;
	while (p + 2 <= end) {
		unsigned b0, cmd;
		p++; /* delta time */
		b0 = cmf[p++];
		if (b0 != 0xFF) {
			p += 2; /* note event (gate + velocity/octave); unused by wave tracks */
			continue;
		}
		if (p >= end)
			break;
		cmd = cmf[p++];
		if (cmd == 0xDF)
			break;
		if (cmd >= 0xF1 && cmd <= 0xF4) {
			unsigned l;
			if (p + 2 > end)
				break;
			l = be16(cmf + p);
			if (cmd == 0xF1 && l > 7 && p + 2 + l <= end && be16(cmf + p + 2) == 0x0044) {
				const uint8_t *q = cmf + p + 2 + 7, *qe = cmf + p + 2 + l;
				while (q < qe) {
					int pay = payload_len(*q);
					if (pay < 0 || q + 1 + pay > qe)
						break;
					memset(padded, 0, sizeof(padded));
					memcpy(padded, q, (size_t)(1 + pay));
					pkt->data = padded;
					pkt->size = 1 + pay;
					if (avcodec_send_packet(ctx, pkt) < 0 || drain(ctx, fr, &out) != 0)
						goto done;
					q += 1 + pay;
				}
			}
			p += 2 + l;
		} else {
			if (cmd == 0xE8 && p < end)
				waveVolume = cmf[p] & 0x3f;
			p++; /* one-byte parameter */
		}
	}
	pkt->data = NULL;
	pkt->size = 0;
	avcodec_send_packet(ctx, NULL);
	drain(ctx, fr, &out);
	if (waveVolume < 63)
		for (int k = 0; k < out.n; k++)
			out.s[k] = (int16_t)(out.s[k] * waveVolume / 63);
	ok = 0;
done:
	av_frame_free(&fr);
	av_packet_free(&pkt);
	avcodec_free_context(&ctx);
	(void)i;
	if (ok != 0) {
		free(out.s);
		return -1;
	}
	*pcm = out.s;
	*samples = out.n;
	return 0;
}
