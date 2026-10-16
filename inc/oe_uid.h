#ifndef OE_S60_UID_H
#define OE_S60_UID_H

/* Development-range UIDs (0xE0000000-0xEFFFFFFF). Fine for a self-signed
 * SISX; they will never match a Nokia-signed application. Allocate a real
 * UID from Symbian Signed only if you ever get a developer certificate. */
#define OE_UID3  0xE0E1E001
#define OE2_UID3 0xE0E2E002

/* KUidApp / S60 application UID2 is always 0x100039CE. */

#endif
