#ifndef OE2_MODDATA_H
#define OE2_MODDATA_H

/*
 * Read-only access to data tables that the original game keeps inside its
 * executable (orcsandelves2.mod). We never copy those tables into our source:
 * the user supplies their own .mod in gamedata/, we verify it is the release we
 * know the offsets for, and the engine reads the tables from it at startup.
 */

#include <stddef.h>
#include <stdint.h>

/* The supported build: Orcs & Elves II, BREW, EN, EA (Oct 2007). */
#define MOD_EXPECTED_SIZE  292116u
#define MOD_EXPECTED_CRC32 0x1631c7f1u

typedef struct ModData {
	uint8_t *data;
	size_t size;
} ModData;

/* 0 ok, -1 missing/unreadable, -2 wrong build (size/CRC mismatch). */
int ModData_open(ModData *m, const char *path);
void ModData_close(ModData *m);

/* Little-endian readers at a file offset (the module loads at address 0, so a
 * code address and its file offset are the same). Out-of-range reads return 0. */
uint8_t ModData_u8(const ModData *m, uint32_t off);
int8_t ModData_s8(const ModData *m, uint32_t off);
uint16_t ModData_u16(const ModData *m, uint32_t off);
int16_t ModData_s16(const ModData *m, uint32_t off);
uint32_t ModData_u32(const ModData *m, uint32_t off);
int32_t ModData_s32(const ModData *m, uint32_t off);
/* NUL-terminated string at off, or "" if out of range. */
const char *ModData_str(const ModData *m, uint32_t off);
const uint8_t *ModData_ptr(const ModData *m, uint32_t off, size_t len);

/* The one loaded .mod used by the engine. */
extern ModData g_mod;

#endif
