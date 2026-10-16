#include "bar.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

static unsigned rd16(const uint8_t *p) { return p[0] | (p[1] << 8); }
static uint32_t rd32(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }

/* Header (little endian):
 *   0x00 u16 version (0x11)   0x06 u16 number of type-directory entries
 *   0x08 u32 type directory offset (8 bytes each: u16 type, u16 first id, u16 last-first, u16 first index)
 *   0x10 u32 entry offset table   0x18 u32 offset of the first entry (= end of the table)
 * Each data entry: u16 header length (including itself), MIME string, payload. */
int Bar_open(Bar *bar, const char *path)
{
	FILE *f;
	long len;
	unsigned ntypes, k, j;
	uint32_t typedir, tbl, tblend, nent;

	memset(bar, 0, sizeof(*bar));
	f = fopen(path, "rb");
	if (!f)
		return -1;
	if (fseek(f, 0, SEEK_END) != 0 || (len = ftell(f)) < 0x20 || fseek(f, 0, SEEK_SET) != 0) {
		fclose(f);
		return -1;
	}
	bar->data = malloc((size_t)len);
	if (!bar->data || fread(bar->data, 1, (size_t)len, f) != (size_t)len) {
		fclose(f);
		Bar_close(bar);
		return -1;
	}
	fclose(f);
	bar->size = (size_t)len;

	ntypes = rd16(bar->data + 6);
	typedir = rd32(bar->data + 0x08);
	tbl = rd32(bar->data + 0x10);
	tblend = rd32(bar->data + 0x18);
	/* 64-bit sums so a corrupt header cannot wrap past the checks */
	if ((uint64_t)typedir + (uint64_t)ntypes * 8 > bar->size || tbl >= tblend || tblend > bar->size) {
		Bar_close(bar);
		return -1;
	}
	nent = (tblend - tbl) / 4;
	bar->entries = calloc(nent, sizeof(BarEntry));
	if (!bar->entries) {
		Bar_close(bar);
		return -1;
	}
	for (k = 0; k < ntypes; k++) {
		const uint8_t *t = bar->data + typedir + k * 8;
		unsigned type = rd16(t), first = rd16(t + 2), span = rd16(t + 4), idx = rd16(t + 6);
		if (type != 6)
			continue;
		for (j = 0; j <= span; j++) {
			uint32_t o, e, hl;
			BarEntry *be;
			if (idx + j >= nent || (uint32_t)bar->count >= nent)
				break;
			o = rd32(bar->data + tbl + 4 * (idx + j));
			e = (idx + j + 1 < nent) ? rd32(bar->data + tbl + 4 * (idx + j + 1)) : (uint32_t)bar->size;
			if (bar->size < 2 || o > bar->size - 2 || e > bar->size || e < o)
				continue;
			hl = rd16(bar->data + o);
			if (hl < 2 || hl > e - o)
				continue;
			be = &bar->entries[bar->count++];
			be->id = (int)(first + j);
			be->type = (int)type;
			be->offset = o + hl;
			be->size = e - o - hl;
			/* gzip FNAME */
			if (be->size > 10 && bar->data[be->offset] == 0x1f && bar->data[be->offset + 1] == 0x8b) {
				const uint8_t *g = bar->data + be->offset;
				uint32_t p = 10;
				if (g[3] & 4)
					p = p + 2 <= be->size ? p + 2 + rd16(g + p) : be->size;
				if ((g[3] & 8) && p < be->size) {
					size_t n = 0;
					while (p + n < be->size && g[p + n] && n < sizeof(be->name) - 1)
						n++;
					memcpy(be->name, g + p, n);
				}
			}
		}
	}
	return 0;
}

int Bar_isSupported(const Bar *bar)
{
	return bar->data && bar->size == BAR_EXPECTED_SIZE &&
	       crc32(0L, bar->data, (uInt)bar->size) == BAR_EXPECTED_CRC32;
}

void Bar_close(Bar *bar)
{
	free(bar->data);
	free(bar->entries);
	memset(bar, 0, sizeof(*bar));
}

const BarEntry *Bar_find(const Bar *bar, int id)
{
	for (int i = 0; i < bar->count; i++)
		if (bar->entries[i].id == id)
			return &bar->entries[i];
	return NULL;
}

const BarEntry *Bar_findByName(const Bar *bar, const char *name)
{
	if (*name == '/')
		name++;
	for (int i = 0; i < bar->count; i++)
		if (strcmp(bar->entries[i].name, name) == 0)
			return &bar->entries[i];
	return NULL;
}

static uint8_t *load_entry(const Bar *bar, const BarEntry *e, size_t *outSize)
{
	const uint8_t *src;
	uint8_t *out;
	uint32_t isize;
	z_stream zs;

	if (!e)
		return NULL;
	src = bar->data + e->offset;
	if (e->size < 18 || src[0] != 0x1f || src[1] != 0x8b) {
		out = malloc(e->size ? e->size : 1);
		if (out)
			memcpy(out, src, e->size);
		if (outSize)
			*outSize = e->size;
		return out;
	}
	isize = rd32(src + e->size - 4); /* gzip ISIZE */
	out = malloc(isize ? isize : 1);
	if (!out)
		return NULL;
	memset(&zs, 0, sizeof(zs));
	if (inflateInit2(&zs, 16 + MAX_WBITS) != Z_OK) {
		free(out);
		return NULL;
	}
	zs.next_in = (Bytef *)src;
	zs.avail_in = e->size;
	zs.next_out = out;
	zs.avail_out = isize;
	if (inflate(&zs, Z_FINISH) != Z_STREAM_END || zs.total_out != isize) {
		inflateEnd(&zs);
		free(out);
		return NULL;
	}
	inflateEnd(&zs);
	if (outSize)
		*outSize = isize;
	return out;
}

uint8_t *Bar_load(const Bar *bar, int id, size_t *outSize)
{
	return load_entry(bar, Bar_find(bar, id), outSize);
}

uint8_t *Bar_loadByName(const Bar *bar, const char *name, size_t *outSize)
{
	return load_entry(bar, Bar_findByName(bar, name), outSize);
}
