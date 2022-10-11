#include "id3.h"
#include "unpack_u32be.h"

#include <string.h>

static void pack_uint32_syncsafe(uint8_t *output, uint32_t val) {
    output[0] = (uint8_t)(((val >> 21) & 0x7F));
    output[1] = (uint8_t)(((val >> 14) & 0x7F));
    output[2] = (uint8_t)(((val >>  7) & 0x7F));
    output[3] = (uint8_t)(((val >>  0) & 0x7F));
}

void id3_init(id3* id3) {
    membuf_init(id3);
}

void id3_reset(id3* id3) {
    id3->len = 10;
}

void id3_free(id3* id3) {
    membuf_free(id3);
}

/* done during open not init */
int id3_ready(id3* id3) {
    int r;
    if( (r = membuf_ready(id3,10)) != 0) return r;
    id3->len = 10;

    id3->x[0] = 'I';
    id3->x[1] = 'D';
    id3->x[2] = '3';
    id3->x[3] = 0x04;
    id3->x[4] = 0x00;
    id3->x[5] = 0x00;
    id3->x[6] = 0x00;
    id3->x[7] = 0x00;
    id3->x[8] = 0x00;
    id3->x[9] = 0x00;
    return 0;
}

static int id3_encode_apic_frame(id3* id3, const tag* t) {
    /* an incoming "APIC" frame is really a FLAC-style
     * metadata_picture_block that will need to be converted 
     * a bit */

    /* the length for this is going to be:
     * 1 text encoding byte
     * length of the mime type + null byte
     * 1 picture type byte
     * length of the description + null byte
     * actual picture data */
    int r;
    size_t len;
    uint32_t mime_len, desc_len, pic_len;

    mime_len = unpack_u32be(&t->value.x[4]);
    desc_len = unpack_u32be(&t->value.x[4 + 4 + mime_len]);
    pic_len  = unpack_u32be(&t->value.x[4 + 4 + mime_len + 4 + desc_len + 4 + 4 + 4 +4]);

    len = 1 + mime_len + 1 + 1 + desc_len + 1 + pic_len;
    /*   text + mimestring + null + type + desc_string + null + data */

    if( (r = membuf_readyplus(id3,len)) != 0) return r;
    /* text-encoding byte, hard-coded to utf-8 */
    id3->x[id3->len++] = 0x03;

    /* mime string */
    memcpy(&id3->x[id3->len],&t->value.x[4 + 4], mime_len);
    id3->len += mime_len;
    id3->x[id3->len++] = 0x00;

    /* picture type */
    id3->x[id3->len++] = (uint8_t)unpack_u32be(&t->value.x[0]);

    /* description string */
    if(desc_len > 0) {
        memcpy(&id3->x[id3->len],&t->value.x[4 + 4 + mime_len + 4], desc_len);
        id3->len += desc_len;
    }
    id3->x[id3->len++] = 0x00;

    /* picture data */
    memcpy(&id3->x[id3->len],&t->value.x[4 + 4 + mime_len + 4 + desc_len + 4 + 4 +4 +4 + 4], pic_len);
    id3->len += pic_len;

    return 0;
}

static int id3_encode_priv_com_apple_streaming_transportStreamTimestamp_frame(id3* id3, const tag* t) {
    int r;

    if( (r = membuf_readyplus(id3,53)) != 0) return r; /* 8 bytes for integer, strlen("com.apple.streaming.transportStreamTimestamp") == 44, +1 null */

    membuf_append(id3,(uint8_t*)"com.apple.streaming.transportStreamTimestamp",45);
    membuf_append(id3,t->value.x,t->value.len);
    return 0;
}

static int id3_encode_text_frame(id3* id3, const tag* t) {
    int r;
    size_t len;

    /* for text frames, the length will be:
     * 1 encoding byte
     * text string + null byte
     * if the string is a TXXX frame (which we
     * detect by just checking if the key length > 4,
     * since we mark those as "TXXX:stuff")
     * then we also have (key length) - 5, and another null byte */
    len = 1 + t->value.len + 1;
    if(t->key.len > 4) len += (t->key.len - 4);
    if( (r = membuf_readyplus(id3,len)) != 0) return r;

    /* text-encoding byte, hard-coded to utf-8 */
    id3->x[id3->len++] = 0x03;
    if(t->key.len > 4) {
        membuf_append(id3,&t->key.x[5],t->key.len - 5);
        id3->x[id3->len++] = 0x00;
    }
    membuf_append(id3,t->value.x,t->value.len);
    id3->x[id3->len++] = 0x00;
    return 0;
}

static int id3_encode_tag(id3* id3, const tag* t) {
    if(t->key.x[0] == 'T' ||
       strbuf_equals_cstr(&t->key,"GRP1") ||
       strbuf_equals_cstr(&t->key,"MVNM") ||
       strbuf_equals_cstr(&t->key,"MVIN") ||
       strbuf_equals_cstr(&t->key,"USLT")) {
        return id3_encode_text_frame(id3,t);
    }

    if(strbuf_equals_cstr(&t->key,"APIC")) return id3_encode_apic_frame(id3,t);

    if(strbuf_equals_cstr(&t->key,"PRIV:com.apple.streaming.transportStreamTimestamp"))
        return id3_encode_priv_com_apple_streaming_transportStreamTimestamp_frame(id3,t);

    return -1;
}

int id3_add_tag(id3* id3, const tag* t) {
    int r;
    size_t len = 0;
    size_t pos = id3->len;

    if( (r = membuf_readyplus(id3,10)) != 0) return r;

    membuf_append(id3,t->key.x,4);
    /* 4 bytes for size */
    id3->len += 4;

    /* 2 bytes for flags */
    id3->x[id3->len++] = 0;
    id3->x[id3->len++] = 0;

    if( (r = id3_encode_tag(id3, t)) != 0) return r;
    len = id3->len - pos - 10;
    pack_uint32_syncsafe(&id3->x[pos+4],(uint32_t)len);
    pack_uint32_syncsafe(&id3->x[6],(uint32_t)id3->len - 10);
    return 0;
}


int id3_add_taglist(id3* id3, const taglist *list) {
    size_t i = 0;
    size_t len = taglist_len(list);
    tag* t;
    int r;

    for(i=0; i < len; i++) {
        t = taglist_get_tag(list,i);
        if( (r = id3_add_tag(id3,t)) != 0) return r;
    }
    return 0;
}
