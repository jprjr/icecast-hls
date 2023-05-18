/* SPDX-License-Identifier: 0BSD */
/* License text available at end of this file */
#ifndef MINIOGGH
#define MINIOGGH

#if !defined(MINIOGG_API)
    #ifdef MINIOGG_DLL
        #ifdef _WIN32
            #define MINIOGG_DLL_IMPORT  __declspec(dllimport)
            #define MINIOGG_DLL_EXPORT  __declspec(dllexport)
        #else
            #if defined(__GNUC__) && __GNUC__ >= 4
                #define MINIOGG_DLL_IMPORT  __attribute__((visibility("default")))
                #define MINIOGG_DLL_EXPORT  __attribute__((visibility("default")))
            #else
                #define MINIOGG_DLL_IMPORT
                #define MINIOGG_DLL_EXPORT
            #endif
        #endif

        #ifdef MINIOGG_IMPLEMENTATION
            #define MINIOGG_API  MINIOGG_DLL_EXPORT
        #else
            #define MINIOGG_API  MINIOGG_DLL_IMPORT
        #endif
    #else
        #define MINIOGG_API extern
    #endif
#endif

#define MINIOGG_MAX_PAGE 65307
#define MINIOGG_MAX_SEGMENTS 255
#define MINIOGG_SEGMENT_SIZE 255
#define MINIOGG_HEADER_SIZE 27
#define MINIOGG_MAX_HEADER (MINIOGG_MAX_SEGMENTS + MINIOGG_HEADER_SIZE) /* 282 */
#define MINIOGG_MAX_BODY (MINIOGG_MAX_SEGMENTS * MINIOGG_SEGMENT_SIZE) /* 65025 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

struct miniogg {
    /* these are intended to be read-only fields */

    /* header and body data along with lengths,
     * it's only valid to read these after calling
     * miniogg_finish_page() */
    uint8_t header[MINIOGG_MAX_HEADER];
    uint8_t body[MINIOGG_MAX_BODY];
    uint32_t header_len;
    uint32_t body_len;

    /* the granulepos, segment, and pageno are all managed
     * automatically */
    uint64_t granulepos;
    uint32_t segment;
    uint32_t pageno;
    uint32_t packets;

    /* these fields can be set by the user */

    /* every ogg bitstream should have a unique serialno */
    uint32_t serialno;

    /* these fields are automatically cleared
     * after calling miniogg_finish_page(). Also,
     * bos is set to 1 during init */
    uint8_t bos;
    uint8_t eos;

    /* the continuation flag is autoset to 1 if miniogg_finish_page()
     * detects that the packet spans multiple pages */
    uint8_t continuation;
};

typedef struct miniogg miniogg;

/* resets all fields to default values and sets a serial number */
MINIOGG_API
void miniogg_init(miniogg* p, uint32_t serialno);

/* returns 0 if packet was added fully, 1 if continuation is needed,
 * the number of bytes read is returned in used */
MINIOGG_API
int miniogg_add_packet(miniogg* p, const void* data, size_t len, uint64_t granulepos, size_t *used);

/* encodes flags, granulepos, etc, calculates the crc32,
 * sets the header_len and body_len fields, increases pageno
 * and resets the bos/eos/continuation flags */
MINIOGG_API
void miniogg_finish_page(miniogg* p);

/* similar to miniogg_finish_page but sets the end-of-stream flag to true */
MINIOGG_API
void miniogg_eos(miniogg* p);

/* returns how large the ogg page would be if
 * written out right now (header length + body length) */
MINIOGG_API
uint32_t miniogg_used_space(const miniogg* p);

/* returns how much space is available for data in the current page,
 * without having to continue into another page. */
MINIOGG_API
uint32_t miniogg_available_space(const miniogg* p);


#ifdef __cplusplus
}
#endif

#endif

#ifdef MINIOGG_IMPLEMENTATION

#include <string.h>

static const uint32_t crc32_table[256] = {
  0x00000000, 0x04C11DB7L, 0x09823B6EL, 0x0D4326D9L,
  0x130476DC, 0x17C56B6BL, 0x1A864DB2L, 0x1E475005L,
  0x2608EDB8, 0x22C9F00FL, 0x2F8AD6D6L, 0x2B4BCB61L,
  0x350C9B64, 0x31CD86D3L, 0x3C8EA00AL, 0x384FBDBDL,
  0x4C11DB70, 0x48D0C6C7L, 0x4593E01EL, 0x4152FDA9L,
  0x5F15ADAC, 0x5BD4B01BL, 0x569796C2L, 0x52568B75L,
  0x6A1936C8, 0x6ED82B7FL, 0x639B0DA6L, 0x675A1011L,
  0x791D4014, 0x7DDC5DA3L, 0x709F7B7AL, 0x745E66CDL,
  0x9823B6E0, 0x9CE2AB57L, 0x91A18D8EL, 0x95609039L,
  0x8B27C03C, 0x8FE6DD8BL, 0x82A5FB52L, 0x8664E6E5L,
  0xBE2B5B58, 0xBAEA46EFL, 0xB7A96036L, 0xB3687D81L,
  0xAD2F2D84, 0xA9EE3033L, 0xA4AD16EAL, 0xA06C0B5DL,
  0xD4326D90, 0xD0F37027L, 0xDDB056FEL, 0xD9714B49L,
  0xC7361B4C, 0xC3F706FBL, 0xCEB42022L, 0xCA753D95L,
  0xF23A8028, 0xF6FB9D9FL, 0xFBB8BB46L, 0xFF79A6F1L,
  0xE13EF6F4, 0xE5FFEB43L, 0xE8BCCD9AL, 0xEC7DD02DL,
  0x34867077, 0x30476DC0L, 0x3D044B19L, 0x39C556AEL,
  0x278206AB, 0x23431B1CL, 0x2E003DC5L, 0x2AC12072L,
  0x128E9DCF, 0x164F8078L, 0x1B0CA6A1L, 0x1FCDBB16L,
  0x018AEB13, 0x054BF6A4L, 0x0808D07DL, 0x0CC9CDCAL,
  0x7897AB07, 0x7C56B6B0L, 0x71159069L, 0x75D48DDEL,
  0x6B93DDDB, 0x6F52C06CL, 0x6211E6B5L, 0x66D0FB02L,
  0x5E9F46BF, 0x5A5E5B08L, 0x571D7DD1L, 0x53DC6066L,
  0x4D9B3063, 0x495A2DD4L, 0x44190B0DL, 0x40D816BAL,
  0xACA5C697, 0xA864DB20L, 0xA527FDF9L, 0xA1E6E04EL,
  0xBFA1B04B, 0xBB60ADFCL, 0xB6238B25L, 0xB2E29692L,
  0x8AAD2B2F, 0x8E6C3698L, 0x832F1041L, 0x87EE0DF6L,
  0x99A95DF3, 0x9D684044L, 0x902B669DL, 0x94EA7B2AL,
  0xE0B41DE7, 0xE4750050L, 0xE9362689L, 0xEDF73B3EL,
  0xF3B06B3B, 0xF771768CL, 0xFA325055L, 0xFEF34DE2L,
  0xC6BCF05F, 0xC27DEDE8L, 0xCF3ECB31L, 0xCBFFD686L,
  0xD5B88683, 0xD1799B34L, 0xDC3ABDEDL, 0xD8FBA05AL,
  0x690CE0EE, 0x6DCDFD59L, 0x608EDB80L, 0x644FC637L,
  0x7A089632, 0x7EC98B85L, 0x738AAD5CL, 0x774BB0EBL,
  0x4F040D56, 0x4BC510E1L, 0x46863638L, 0x42472B8FL,
  0x5C007B8A, 0x58C1663DL, 0x558240E4L, 0x51435D53L,
  0x251D3B9E, 0x21DC2629L, 0x2C9F00F0L, 0x285E1D47L,
  0x36194D42, 0x32D850F5L, 0x3F9B762CL, 0x3B5A6B9BL,
  0x0315D626, 0x07D4CB91L, 0x0A97ED48L, 0x0E56F0FFL,
  0x1011A0FA, 0x14D0BD4DL, 0x19939B94L, 0x1D528623L,
  0xF12F560E, 0xF5EE4BB9L, 0xF8AD6D60L, 0xFC6C70D7L,
  0xE22B20D2, 0xE6EA3D65L, 0xEBA91BBCL, 0xEF68060BL,
  0xD727BBB6, 0xD3E6A601L, 0xDEA580D8L, 0xDA649D6FL,
  0xC423CD6A, 0xC0E2D0DDL, 0xCDA1F604L, 0xC960EBB3L,
  0xBD3E8D7E, 0xB9FF90C9L, 0xB4BCB610L, 0xB07DABA7L,
  0xAE3AFBA2, 0xAAFBE615L, 0xA7B8C0CCL, 0xA379DD7BL,
  0x9B3660C6, 0x9FF77D71L, 0x92B45BA8L, 0x9675461FL,
  0x8832161A, 0x8CF30BADL, 0x81B02D74L, 0x857130C3L,
  0x5D8A9099, 0x594B8D2EL, 0x5408ABF7L, 0x50C9B640L,
  0x4E8EE645, 0x4A4FFBF2L, 0x470CDD2BL, 0x43CDC09CL,
  0x7B827D21, 0x7F436096L, 0x7200464FL, 0x76C15BF8L,
  0x68860BFD, 0x6C47164AL, 0x61043093L, 0x65C52D24L,
  0x119B4BE9, 0x155A565EL, 0x18197087L, 0x1CD86D30L,
  0x029F3D35, 0x065E2082L, 0x0B1D065BL, 0x0FDC1BECL,
  0x3793A651, 0x3352BBE6L, 0x3E119D3FL, 0x3AD08088L,
  0x2497D08D, 0x2056CD3AL, 0x2D15EBE3L, 0x29D4F654L,
  0xC5A92679, 0xC1683BCEL, 0xCC2B1D17L, 0xC8EA00A0L,
  0xD6AD50A5, 0xD26C4D12L, 0xDF2F6BCBL, 0xDBEE767CL,
  0xE3A1CBC1, 0xE760D676L, 0xEA23F0AFL, 0xEEE2ED18L,
  0xF0A5BD1D, 0xF464A0AAL, 0xF9278673L, 0xFDE69BC4L,
  0x89B8FD09, 0x8D79E0BEL, 0x803AC667L, 0x84FBDBD0L,
  0x9ABC8BD5, 0x9E7D9662L, 0x933EB0BBL, 0x97FFAD0CL,
  0xAFB010B1, 0xAB710D06L, 0xA6322BDFL, 0xA2F33668L,
  0xBCB4666D, 0xB8757BDAL, 0xB5365D03L, 0xB1F740B4L
};

static inline uint32_t crc32(uint32_t crc, const uint8_t* buf, size_t len) {
    const uint8_t* e = buf + len;

    while(buf < e) {
        crc = (crc << 8) ^ crc32_table[( (crc >> 24) & 0xff) ^ (*buf++)];
    }

    return crc;
}

static inline void miniogg_pack_u32le(uint8_t* d, uint32_t n) {
    d[0] = (uint8_t)(( n       ) & 0xFF);
    d[1] = (uint8_t)(( n >> 8  ) & 0xFF);
    d[2] = (uint8_t)(( n >> 16 ) & 0xFF);
    d[3] = (uint8_t)(( n >> 24 ) & 0xFF);
}

static inline void miniogg_pack_u64le(uint8_t* d, uint64_t n) {
    d[0] = (uint8_t)(( n       ) & 0xFF);
    d[1] = (uint8_t)(( n >> 8  ) & 0xFF);
    d[2] = (uint8_t)(( n >> 16 ) & 0xFF);
    d[3] = (uint8_t)(( n >> 24 ) & 0xFF);
    d[4] = (uint8_t)(( n >> 32 ) & 0xFF);
    d[5] = (uint8_t)(( n >> 40 ) & 0xFF);
    d[6] = (uint8_t)(( n >> 48 ) & 0xFF);
    d[7] = (uint8_t)(( n >> 56 ) & 0xFF);
}

static inline void miniogg_set_granulepos(miniogg* p, uint64_t granulepos) {
    miniogg_pack_u64le(&p->header[6],granulepos);
}

static inline void miniogg_set_pageno(miniogg* p, uint32_t pageno) {
    miniogg_pack_u32le(&p->header[18],pageno);
}

static inline void miniogg_set_crc(miniogg* p, uint32_t crc) {
    miniogg_pack_u32le(&p->header[22],crc);
}

static inline void miniogg_set_serialno(miniogg* p, uint32_t serialno) {
    miniogg_pack_u32le(&p->header[14],serialno);
}

static inline uint32_t miniogg_used_space__inline(const miniogg* p) {
    uint32_t len = 0;
    uint32_t i = 0;
    uint32_t segments = p->segment;
    for(i=0;i<segments;i++) {
        len += (uint32_t)p->header[MINIOGG_HEADER_SIZE+i];
    }
    return len;
}

MINIOGG_API
void miniogg_init(miniogg* p, uint32_t serialno) {
    memset(p->header,0,MINIOGG_HEADER_SIZE);
    p->header_len = 0;
    p->body_len = 0;

    p->bos = 1;
    p->eos = 0;
    p->continuation = 0;
    p->granulepos = ~0ULL;
    p->pageno = 0;
    p->serialno = serialno;

    p->segment = 0;
    p->packets = 0;

    p->header[0] = 'O';
    p->header[1] = 'g';
    p->header[2] = 'g';
    p->header[3] = 'S';
}

MINIOGG_API
uint32_t miniogg_used_space(const miniogg* p) {
    return miniogg_used_space__inline(p) + p->segment + MINIOGG_HEADER_SIZE;
}

MINIOGG_API
uint32_t miniogg_available_space(const miniogg* p) {
    if(p->segment >= MINIOGG_MAX_SEGMENTS) return 0;
    return MINIOGG_MAX_BODY - ( ((uint32_t)p->segment) * MINIOGG_SEGMENT_SIZE) - 1;
}

MINIOGG_API
int miniogg_add_packet(miniogg* p, const void* data, size_t len, uint64_t granulepos, size_t *used) {
    int full;
    size_t l = 0;
    size_t u = 0;
    uint32_t slot = p->segment;
    uint32_t slots = (uint32_t)( (len / MINIOGG_MAX_SEGMENTS) + 1);

    if( (full = (slot == MINIOGG_MAX_SEGMENTS))) {
        *used = 0;
        return 1;
    }

    l = miniogg_used_space__inline(p);

    while(slots) {
        uint8_t b = len > MINIOGG_SEGMENT_SIZE ? MINIOGG_SEGMENT_SIZE : (uint8_t)len;
        p->header[MINIOGG_HEADER_SIZE + slot++] = b;

        len -= b;
        u += b;
        slots--;

        if( (full = (slot == MINIOGG_MAX_SEGMENTS))) break;
    }

    if(u) {
        memcpy(&p->body[l],data,u);
    }

    if(slots == 0) { /* we finished writing this packet, record the granule position */
        p->granulepos = granulepos;
        p->packets++;
    } else if(full && p->segment == 0) {/* we spanned the whole page */
        p->granulepos = ~0ULL;
    }

    p->segment = slot;
    *used = u;
    return slots != 0;
}

MINIOGG_API
void miniogg_finish_page(miniogg* p) {
    uint32_t crc = 0;

    if(p->bos) p->pageno = 0;

    p->header[5] = 0 | p->eos << 2 | p->bos << 1 | p->continuation;
    miniogg_set_granulepos(p,p->granulepos);
    miniogg_set_serialno(p,p->serialno);
    miniogg_set_pageno(p,p->pageno++);
    miniogg_set_crc(p,0);
    p->header[26] = (uint8_t)p->segment;

    p->header_len = (size_t)p->segment + MINIOGG_HEADER_SIZE;
    p->body_len = miniogg_used_space__inline(p);

    crc = crc32(crc,p->header,p->header_len);
    crc = crc32(crc,p->body,p->body_len);
    miniogg_set_crc(p,crc);

    if(p->segment == MINIOGG_MAX_SEGMENTS &&
       p->header[MINIOGG_HEADER_SIZE + 254] == MINIOGG_SEGMENT_SIZE) {
        p->continuation = 1;
    } else {
        p->continuation = 0;
    }

    p->segment = 0;
    p->bos = 0;
    p->eos = 0;
    p->packets = 0;
}

MINIOGG_API
void miniogg_eos(miniogg* p) {
    p->eos = 1;
    miniogg_finish_page(p);
}


#endif /* IMPLEMENTATION */

/*
BSD Zero Clause License

Copyright (c) 2023 John Regan

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
PERFORMANCE OF THIS SOFTWARE.
*/
