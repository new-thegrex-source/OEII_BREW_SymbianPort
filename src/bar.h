#ifndef OE2_BAR_H
#define OE2_BAR_H

#include <stddef.h>
#include <stdint.h>

/* BREW resource archive (.bar). Data resources (type 6) carry a MIME string and
 * usually a gzip stream whose header keeps the original file name. */

typedef struct BarEntry {
	int id;          /* resource id, e.g. 1000 entities.bin, 2000 m_01.bsp */
	int type;        /* BREW resource type (6 = data) */
	uint32_t offset; /* payload offset within the archive (after the MIME header) */
	uint32_t size;   /* payload size (possibly compressed) */
	char name[64];   /* original file name from the gzip header, or "" */
} BarEntry;

typedef struct Bar {
	uint8_t *data;
	size_t size;
	BarEntry *entries;
	int count;
} Bar;

/* The one supported Orcs & Elves II release (see README). */
#define BAR_EXPECTED_SIZE  2130702u
#define BAR_EXPECTED_CRC32 0xad370c54u

/* Returns 0 on success. */
int Bar_open(Bar *bar, const char *path);
/* 1 if the archive is exactly the supported release (size and CRC32). */
int Bar_isSupported(const Bar *bar);
void Bar_close(Bar *bar);
const BarEntry *Bar_find(const Bar *bar, int id);
const BarEntry *Bar_findByName(const Bar *bar, const char *name);
/* Decompressed copy of a resource (caller frees). NULL if missing. */
uint8_t *Bar_load(const Bar *bar, int id, size_t *outSize);
uint8_t *Bar_loadByName(const Bar *bar, const char *name, size_t *outSize);

#endif
