#include "dump.h"

#include <assert.h>
#include <string.h>

#include "rcm_usb.h"
#include "lib/printk.h"
#include "z85.h"

#define REMAINING_TEXT_BYTES(bufVar, bufPosVar) ((((int)sizeof(bufVar)-(int)bufPosVar-1) > 0) ? (sizeof(bufVar)-bufPosVar-1) : 0)

// ntohl/ntohll from musl

static uint32_t __bswap_32(uint32_t __x)
{
	return __x>>24 | (__x>>8&0xff00) | (__x<<8&0xff0000) | __x<<24;
}

static uint64_t __bswap_64(uint64_t __x)
{
	return (__bswap_32(__x)+0ULL)<<32 | __bswap_32(__x>>32);
}

#define bswap_32(x) __bswap_32(x)
#define bswap_64(x) __bswap_64(x)

uint32_t htonl(uint32_t n)
{
	union { int i; char c; } u = { 1 };
	return u.c ? bswap_32(n) : n;
}

uint64_t htonll(uint64_t n)
{
	union { int i; char c; } u = { 1 };
	return u.c ? bswap_64(n) : n;
}

void dump_helper(const char name[DUMP_NAME_SIZE], void *addr, uint32_t len, dump_record_t *rec) {
	assert(len <= DUMP_DATA_SIZE);
	assert(strlen(name) + 1 <= DUMP_NAME_SIZE);

	rec->addr = htonll((uintptr_t)addr);
	rec->len = htonl(len);
	memset(rec->name, 0, DUMP_NAME_SIZE);
	strncpy(rec->name, name, DUMP_NAME_SIZE);
	memcpy(rec->data, addr, len);
}

void dump(const char name[DUMP_NAME_SIZE], void *addr, uint32_t len) {
	dump_record_t rec;
	char z85_buf[Z85_ENC_BUF_SIZE(sizeof(rec))];
	char textBuf[4096];
	int currTextBufPos = 0;
    int lastLineLength = 0;

	assert(strlen(name) + 1 <= DUMP_NAME_SIZE);

	lastLineLength = snprintfk(&textBuf[currTextBufPos], REMAINING_TEXT_BYTES(textBuf, currTextBufPos),
		"dumping '%s' at %p size %u\n", name, addr, len);
	currTextBufPos += lastLineLength;
	rcm_usb_device_write_ep1_in_sync((uint8_t*)textBuf, currTextBufPos, NULL);


	while (len > 0) {
		uint32_t rec_data_len = len < DUMP_DATA_SIZE ? len : DUMP_DATA_SIZE;
		dump_helper(name, addr, rec_data_len, &rec);
		len -= rec_data_len;
		addr += rec_data_len;
		char *z85_enc = Z85_encode(
			&rec,
			sizeof(rec) - DUMP_DATA_SIZE + rec_data_len,
			z85_buf, sizeof(z85_buf)
		);
		currTextBufPos = 0;
		lastLineLength = snprintfk(&textBuf[currTextBufPos], REMAINING_TEXT_BYTES(textBuf, currTextBufPos),
			"%s%s\n", DUMP_TAG, z85_enc);
		currTextBufPos += lastLineLength;
		rcm_usb_device_write_ep1_in_sync((uint8_t*)textBuf, currTextBufPos, NULL);
	}
}
