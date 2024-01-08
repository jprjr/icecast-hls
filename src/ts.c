#include "ts.h"

#include <string.h>
#include <limits.h>
#include <assert.h>

#include "bitwriter.h"
#include "pack_u32be.h"

#define DO_ID3 1

static const uint32_t ts_crc32_table[256] = {
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

static
uint32_t ts_crc32(uint32_t crc, const void* buf, size_t len) {
    const uint8_t *b = (const uint8_t *)buf;
    const uint8_t* e = b + len;

    while(b < e) {
        crc = (crc << 8) ^ ts_crc32_table[( (crc >> 24) & 0xff) ^ (*b++)];
    }

    return crc;
}

void mpegts_header_init(mpegts_header *tsh) {
    tsh->tei = 0;
    tsh->pusi = 0;
    tsh->prio = 0;
    tsh->pid = 0;
    tsh->tsc = 0;
    tsh->adapt = 0;
    tsh->cc = 0;
}

void mpegts_adaptation_field_init(mpegts_adaptation_field* f) {
    f->discontinuity = 0;
    f->random_access_error = 0;
    f->es_priority = 0;
    f->pcr_flag = 0;
    f->opcr_flag = 0;
    f->splicing_point_flag = 0;
    f->splice_countdown = 0;
    f->transport_private_data = NULL;
    f->stuffing = 0;
    f->pcr_base = 0;
    f->pcr_extension = 0;
    f->opcr_base = 0;
    f->opcr_extension = 0;
}

void mpegts_stream_init(mpegts_stream *s) {
    s->stream_id = 0;
    s->pts = 0;
    mpegts_header_init(&s->header);
    mpegts_adaptation_field_init(&s->adaptation);
}

uint8_t mpegts_adaptation_field_length(const mpegts_adaptation_field *f) {
    uint8_t len = 1;
    if(f->pcr_flag) len += 6;
    if(f->opcr_flag) len += 6;
    if(f->splicing_point_flag) len += 1;
    if(f->transport_private_data != NULL) len += (uint8_t)f->transport_private_data->len;
    if(f->stuffing) len += f->stuffing;

    return len;
}

int mpegts_packet_reset(membuf* packet, uint8_t fill) {
    int r;

    if( (r = membuf_ready(packet,TS_PACKET_SIZE)) != 0) return r;

    packet->len = 0;
    memset(&packet->x[0], fill, TS_PACKET_SIZE);
    return 0;
}

int mpegts_header_encode(membuf *dest, const mpegts_header *tsh) {
    int r;
    bitwriter bw = BITWRITER_ZERO;

    if( (r = membuf_readyplus(dest,TS_HEADER_SIZE)) != 0) return r;

    bw.buffer = &dest->x[dest->len];
    bw.len = dest->a - dest->len;

    bitwriter_add(&bw, 8, 0x47);

    bitwriter_add(&bw, 1, tsh->tei);
    bitwriter_add(&bw, 1, tsh->pusi);
    bitwriter_add(&bw, 1, tsh->prio);
    bitwriter_add(&bw, 13, tsh->pid);
    bitwriter_add(&bw, 2, tsh->tsc);
    bitwriter_add(&bw, 2, tsh->adapt);
    bitwriter_add(&bw, 4, tsh->cc);
    bitwriter_align(&bw);

    dest->len += bw.pos;

    return 0;
}

int mpegts_adaptation_field_encode(membuf *dest, const mpegts_adaptation_field *f) {
    int r = 0;
    uint8_t len = 0;
    uint8_t stuffing = 0;
    bitwriter bw = BITWRITER_ZERO;

    len = mpegts_adaptation_field_length(f);

    if( (r = membuf_readyplus(dest, len+1)) != 0) return r;

    bw.buffer = &dest->x[dest->len];
    bw.len = dest->a - dest->len;

    bitwriter_add(&bw, 8, len);
    bitwriter_add(&bw, 1, f->discontinuity);
    bitwriter_add(&bw, 1, f->random_access_error);
    bitwriter_add(&bw, 1, f->es_priority);
    bitwriter_add(&bw, 1, f->pcr_flag);
    bitwriter_add(&bw, 1, f->opcr_flag);
    bitwriter_add(&bw, 1, f->splicing_point_flag);
    bitwriter_add(&bw, 1, f->transport_private_data == NULL ? 0 : f->transport_private_data->len ? 1 : 0);
    bitwriter_add(&bw, 1, 0); /* TODO (maybe) adaptation field extension */

    if(f->pcr_flag) {
        bitwriter_add(&bw, 33, f->pcr_base);
        bitwriter_add(&bw, 6, 0x3f);
        bitwriter_add(&bw, 9, f->pcr_extension);
    }

    if(f->opcr_flag) {
        bitwriter_add(&bw, 33, f->opcr_base);
        bitwriter_add(&bw, 6, 0x3f);
        bitwriter_add(&bw, 9, f->opcr_extension);
    }

    if(f->splicing_point_flag) {
        bitwriter_add(&bw, 8, (uint8_t) f->splice_countdown);
    }
    stuffing = f->stuffing;
    while(stuffing--) {
        bitwriter_add(&bw, 8, 0xff);
    }

    bitwriter_align(&bw);

    dest->len += bw.pos;

    return 0;
}


int mpegts_pes_packet_header_encode(membuf* dest, const mpegts_pes_header *header) {
    int r = -1;
    uint8_t stuffing = header->stuffing;
    bitwriter bw = BITWRITER_ZERO;

    if(header->packet_length > TS_MAX_PACKET_SIZE) return r;

    if( (r = membuf_readyplus(dest, 3 + 1 + 2 + 8 + header->stuffing)) != 0) return r;

    bw.buffer = &dest->x[dest->len];
    bw.len = dest->a - dest->len;

    bitwriter_add(&bw, 8, 0x00);
    bitwriter_add(&bw, 8, 0x00);
    bitwriter_add(&bw, 8, 0x01);
    bitwriter_add(&bw, 8, header->stream_id);
    bitwriter_add(&bw, 16, header->packet_length + 8 + header->stuffing);

    /* optional PES header */
    /* covers marker bits (2),
     * scrambling control (2),
     * priority (1),
     * alignment (1)
     * copyright (1)
     * original or copy (1) */
    bitwriter_add(&bw, 8, 0x80);

    /* covers PTS/DTS (2)
     * ESCR flag (1)
     * ES rate flag (1)
     * DSM trick mode flag (1)
     * additional copy info flag (1)
     * CRC flag (1)
     * extension flag (1) */
    bitwriter_add(&bw, 8, 0x80);

    /* number of bytes for the rest of the header */
    bitwriter_add(&bw, 8, 5 + stuffing);

    bitwriter_add(&bw, 8,
      0x21 | ( (header->pts >> 29) & 0x0f));

    bitwriter_add(&bw, 8,
      0x00 | ( (header->pts >> 22) & 0xff));

    bitwriter_add(&bw, 8,
      0x01 | ( (header->pts >> 14) & 0xff));

    bitwriter_add(&bw, 8,
      0x00 | ( (header->pts >>  7) & 0xff));

    bitwriter_add(&bw, 8,
      0x01 | ( (header->pts & 0xff) << 1));

    while(stuffing--) {
        bitwriter_add(&bw, 8, 0xff);
    }

    bitwriter_align(&bw);

    dest->len += bw.pos;

    return 0;
}

int mpegts_pat_encode(membuf* dest, uint16_t program_map_pid) {
    int r;
    uint32_t crc;
    bitwriter bw = BITWRITER_ZERO;

    if( (r = membuf_readyplus(dest, TS_MAX_PAYLOAD_SIZE)) != 0) return r;
    memset(&dest->x[dest->len],0xff,TS_MAX_PAYLOAD_SIZE);

    bw.buffer = &dest->x[dest->len];
    bw.len = dest->a - dest->len;

    bitwriter_add(&bw, 8, 0x00); /* pointer field */
    bitwriter_add(&bw, 8, 0x00); /* table ID (0 for PAT) */
    bitwriter_add(&bw, 1, 0x01); /* section_syntax_indicator */
    bitwriter_add(&bw, 1, 0x00); /* hard-coded to 0 for PAT */
    bitwriter_add(&bw, 2, 0x03); /* reserved bits */
    bitwriter_add(&bw, 2, 0x00); /* unused length bits */
    bitwriter_add(&bw, 10, 13); /* section length, will always be 13 since we hard limit to 1 program */
    bitwriter_add(&bw, 16, 0x0001); /* transport_stream_id, set to 1 */
    bitwriter_add(&bw, 2, 0x03); /* reserved bits */
    bitwriter_add(&bw, 5, 0x00); /* version_number */
    bitwriter_add(&bw, 1, 0x01); /* current_next_indicator */
    bitwriter_add(&bw, 8, 0x00); /* section_number */
    bitwriter_add(&bw, 8, 0x00); /* last_section_number */
    bitwriter_add(&bw, 16, 0x0001); /* program number, set to 1 */
    bitwriter_add(&bw, 3, 0x07); /* reserved bits */
    bitwriter_add(&bw, 13, program_map_pid);
    bitwriter_align(&bw);

    crc = ts_crc32(0xFFFFFFFF, &dest->x[dest->len + 1], 12);
    pack_u32be(&dest->x[dest->len + 13], crc);

    dest->len += 184;
    return 0;
}

int mpegts_pmt_encode(membuf* dest, codec_type codec, uint16_t audio_pid, uint16_t id3_pid) {
    int r;
    uint32_t crc = 0xFFFFFFFF;;
    uint16_t section_length = 13;
    uint8_t stream_type = 0xff;
    bitwriter bw = BITWRITER_ZERO;

    if(id3_pid) section_length += 37; /* 17 bytes for progrm info, 20 bytes for es info */

    switch(codec) {
        case CODEC_TYPE_MP3: {
            stream_type = 0x03;
            section_length += 5;
            break;
        }
        case CODEC_TYPE_AAC: {
            stream_type = 0x0f;
            section_length += 5;
            break;
        }
        case CODEC_TYPE_AC3: {
            stream_type = 0x81;
            section_length += 11;
            break;
        }
        case CODEC_TYPE_EAC3: {
            stream_type = 0x87;
            section_length += 11;
            break;
        }
        default: return -1;
    }

    if( (r = membuf_readyplus(dest, TS_MAX_PAYLOAD_SIZE)) != 0) return r;
    memset(&dest->x[dest->len],0xff,TS_MAX_PAYLOAD_SIZE);

    bw.buffer = &dest->x[dest->len];
    bw.len = dest->a - dest->len;

    bitwriter_add(&bw, 8, 0x00); /* pointer field */
    bitwriter_add(&bw, 8, 0x02); /* table ID (2 for PMT) */
    bitwriter_add(&bw, 1, 0x01); /* section_syntax_indicator */
    bitwriter_add(&bw, 1, 0x00); /* hard-coded to 0 for PMT */
    bitwriter_add(&bw, 2, 0x03); /* reserved bits */
    bitwriter_add(&bw, 12, section_length); /* section length */
    bitwriter_add(&bw, 16, 0x0001); /* program_number (set to 1) */
    bitwriter_add(&bw, 2, 0x03); /* reserved bits */
    bitwriter_add(&bw, 5, 0x00); /* version_number */
    bitwriter_add(&bw, 1, 0x01); /* current_next_indicator */
    bitwriter_add(&bw, 8, 0x00); /* section_number */
    bitwriter_add(&bw, 8, 0x00); /* last_section_number */

    bitwriter_add(&bw, 3, 0x07); /* reserved bits */
    bitwriter_add(&bw, 13, audio_pid); /* PCR PID, we just assume the audio_pid is the main PCR */
    bitwriter_add(&bw, 4, 0x0F); /* reserved bits */
    bitwriter_add(&bw, 12, id3_pid ? 17 : 0); /* program_info_length */

    if(id3_pid) {
        /* begin program_info for timed_id3 */
        bitwriter_add(&bw, 8, 0x25); /* descriptor tag */
        bitwriter_add(&bw, 8, 15); /* descriptor length */
        bitwriter_add(&bw, 16, 0xFFFF); /* metadata application format */
        bitwriter_add(&bw, 32, 0x49443320); /* 'ID3 ' */
        bitwriter_add(&bw, 8, 0xFF); /* metadata format */
        bitwriter_add(&bw, 32, 0x49443320); /* format ID ('ID3 ') */
        bitwriter_add(&bw, 8, 0); /* metadata service ID */
        bitwriter_add(&bw, 1, 0); /* metadata_locator_record_flag */
        bitwriter_add(&bw, 2, 0); /* MPEG_carriage_flags */
        bitwriter_add(&bw, 5, 0x1F); /* reserved */
        bitwriter_add(&bw, 16, 0x0001); /* program number */
    }

    /* that ends descriptors, now for components */
    /* our audio stream */
    bitwriter_add(&bw, 8, stream_type);
    bitwriter_add(&bw, 3, 0x07);
    bitwriter_add(&bw, 13, audio_pid);
    bitwriter_add(&bw, 4, 0x0f);

    switch(codec) {
        case CODEC_TYPE_MP3: /* fall-through */
        case CODEC_TYPE_AAC: {
            bitwriter_add(&bw, 12, 0); /* elementary stream info length */
            break;
        }
        case CODEC_TYPE_AC3: {
            bitwriter_add(&bw, 12, 6); /* elementary stream info length */
            bitwriter_add(&bw, 8, 0x05); /* descriptor tag */
            bitwriter_add(&bw, 8, 4); /* descriptor length */
            bitwriter_add(&bw, 32, 0x41432D33); /* text "AC-3" */
            break;
        }
        case CODEC_TYPE_EAC3: {
            bitwriter_add(&bw, 12, 6); /* elementary stream info length */
            bitwriter_add(&bw, 8, 0x05); /* descriptor tag */
            bitwriter_add(&bw, 8, 4); /* descriptor length */
            bitwriter_add(&bw, 32, 0x45414333); /* text "EAC3" */
            break;
        }

        default: break;
    }

    if(id3_pid) {
        /* our ID3 stream */
        bitwriter_add(&bw, 8, 0x15); /* metadata in PES packets */
        bitwriter_add(&bw, 3, 0x07);
        bitwriter_add(&bw, 13, id3_pid);
        bitwriter_add(&bw, 4, 0x0f);
        bitwriter_add(&bw, 12, 15); /* elementary stream info length */
        bitwriter_add(&bw, 8, 0x26); /* descriptor tag */
        bitwriter_add(&bw, 8, 13); /* descriptor length */
        bitwriter_add(&bw, 16, 0xFFFF); /* metadata application format */
        bitwriter_add(&bw, 32, 0x49443320); /* 'ID3 ' */
        bitwriter_add(&bw, 8, 0xFF); /* metadata format */
        bitwriter_add(&bw, 32, 0x49443320); /* format ID ('ID3 ') */
        bitwriter_add(&bw, 8, 0); /* metadata service ID */
        bitwriter_add(&bw, 3, 0); /* decoder config flags */
        bitwriter_add(&bw, 1, 0); /* DSM-CC flag */
        bitwriter_add(&bw, 4, 0x0F); /* reserved */
    }

    bitwriter_align(&bw);

    crc = ts_crc32(crc, &dest->x[dest->len + 1], section_length - 1);
    pack_u32be(&dest->x[dest->len + section_length], crc);

    dest->len += 184;
    return 0;
}


int mpegts_stream_encode_packet(membuf* dest, mpegts_stream *stream, const membuf* data) {
    int r;
    size_t rem = data->len;
    size_t pos = 0;
    size_t cur = rem;

    /* PES header will be 14 bytes so our max payload if we don't need an adaptation field
     * is 184 - 14 */
    size_t max = TS_MAX_PAYLOAD_SIZE - 14;

    mpegts_pes_header header;

    if(cur > TS_MAX_PACKET_SIZE) return -1;

    stream->header.pusi = 1;
    stream->header.adapt = 0x01;
    stream->adaptation.stuffing = 0;

    header.stream_id  = stream->stream_id;
    header.packet_length = data->len;
    header.stuffing = 0;
    /* FFMPEG sets PTS and PCR offset, I'm not sure how they calculate it so I'll
     * just do hard-coded values of roughly .7 seconds each, that seems to be the
     * default. */
    header.pts = stream->pts + 126000;

    if(stream->adaptation.pcr_flag) {
        stream->header.adapt = 0x03;
        stream->adaptation.pcr_base = stream->pts + 63000;
        /* the PCR flag implies adaptation field so our new max needs 8 bytes removed */
        max -= 8;
    }

    if(cur < max){
        header.stuffing = max - cur;
    }

    if( (r = mpegts_header_encode(dest,&stream->header)) != 0) return r;
    if(stream->header.adapt == 0x03) {
        if( (r = mpegts_adaptation_field_encode(dest,&stream->adaptation)) != 0) return r;
    }
    if( (r = mpegts_pes_packet_header_encode(dest, &header)) != 0) return r;

    if(cur > max) {
        cur = max;
    }

    if( (r = membuf_append(dest, &data->x[pos], cur)) != 0) return r;

    rem -= cur;
    pos += cur;

    stream->header.pusi = 0;
    stream->adaptation.pcr_flag = 0;

    stream->header.cc = (stream->header.cc + 1) & 0xff;

    while(rem) {
        cur = rem;
        max = TS_MAX_PAYLOAD_SIZE;
        stream->header.adapt = 0x01;
        stream->adaptation.stuffing = 0;

        if(cur < max) {
            stream->header.adapt = 0x03;
            max -= 2;

            if(cur < max) {
                stream->adaptation.stuffing = max - cur;
            }
        }

        if(cur > max) {
            cur = max;
        }

        if( (r = mpegts_header_encode(dest,&stream->header)) != 0) return r;
        if(stream->header.adapt == 0x03) {
            if( (r = mpegts_adaptation_field_encode(dest,&stream->adaptation)) != 0) return r;
        }
        if( (r = membuf_append(dest, &data->x[pos], cur)) != 0) return r;

        rem -= cur;
        pos += cur;
        stream->header.cc = (stream->header.cc + 1) & 0xff;
    }

    return 0;
}

