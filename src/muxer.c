#include "muxer.h"
#include "unpack_u32be.h"
#include "pack_u32be.h"

#include <stdio.h>
#include <string.h>

/* provide a default segment handler that just makes everything quit */
static int muxer_default_segment_handler(void* userdata, const segment* seg) {
    (void)userdata;
    (void)seg;
    fprintf(stderr,"[muxer] segment handler not set\n");
    return -1;
}


static int muxer_default_picture_handler(void* userdata, const picture* src, picture* out) {
    (void)userdata;
    (void)src;
    (void)out;
    fprintf(stderr,"[muxer] picture handler not set\n");
    return -1;
}

static int muxer_default_outputconfig_handler(void* userdata, const outputconfig* c) {
    (void)userdata;
    (void)c;
    fprintf(stderr,"[muxer] outputconfig handler not set\n");
    return -1;
}

int muxer_set_segment_handler(muxer* m, const segment_handler* s) {
    m->segment_handler = *s;
    return 0;
}

int muxer_set_picture_handler(muxer* m, const picture_handler* p) {
    m->picture_handler = *p;
    return 0;
}

int muxer_set_outputconfig_handler(muxer* m, const outputconfig_handler* o) {
    m->outputconfig_handler = *o;
    return 0;
}

void muxer_init(muxer* m) {
    m->userdata = NULL;
    m->plugin = NULL;
    m->segment_handler.cb = muxer_default_segment_handler;
    m->segment_handler.userdata = NULL;
    m->picture_handler.cb = muxer_default_picture_handler;
    m->picture_handler.userdata = NULL;
    m->outputconfig_handler.submit = muxer_default_outputconfig_handler;
    m->outputconfig_handler.userdata = NULL;
    m->inband_images = 0;
}

void muxer_free(muxer* m) {
    if(m->userdata != NULL) {
        m->plugin->close(m->userdata);
    }
    m->userdata = NULL;
    m->plugin = NULL;
}

int muxer_create(muxer* m, const strbuf* name) {
    const muxer_plugin* plug;
    void* userdata;

    plug = muxer_plugin_get(name);
    if(plug == NULL) return -1;

    userdata = plug->create();
    if(userdata == NULL) return -1;

    m->userdata = userdata;
    m->plugin = plug;

    return 0;
}

int muxer_open(const muxer* m, const muxerconfig* c) {
    if(m->plugin == NULL || m->userdata == NULL) {
        fprintf(stderr,"[muxer] unable to open: plugin not selected\n");
        return -1;
    }
    return m->plugin->open(m->userdata, c, &m->outputconfig_handler);
}

int muxer_config(const muxer* m, const strbuf* name, const strbuf* value) {
    return m->plugin->config(m->userdata,name,value);
}

int muxer_global_init(void) {
    return muxer_plugin_global_init();
}

void muxer_global_deinit(void) {
    return muxer_plugin_global_deinit();
}

int muxer_submit_dsi(const muxer* m, const membuf* dsi) {
    return m->plugin->submit_dsi(m->userdata, dsi, &m->segment_handler);
}

int muxer_submit_packet(const muxer* m, const packet* p) {
    return m->plugin->submit_packet(m->userdata, p, &m->segment_handler);
}

int muxer_submit_tags(const muxer* m, const taglist* tags) {
    int r = 0;
    size_t apic_idx = 0;
    uint32_t mime_len = 0;
    uint32_t desc_len = 0;
    uint32_t pic_len = 0;
    picture src = PICTURE_ZERO;
    picture dest = PICTURE_ZERO; /* needs to be free'd */
    taglist list; /* will be used as a shallow taglist, will need to be free'd */

    const tag* t;
    tag tmp_tag;

    if( (apic_idx = taglist_find_cstr(tags,"APIC",0)) == taglist_len(tags) || m->inband_images) {
        return m->plugin->submit_tags(m->userdata,tags);
    }

    taglist_init(&list);
    if(taglist_shallow_copy(&list,tags) != 0) return -1;
    /* NOTE list has to be freed now! */

    taglist_remove_tag(&list,apic_idx);
    t = taglist_get_tag(tags,apic_idx);

    /* let's get our meta-info from the picture */
    mime_len = unpack_u32be(&t->value.x[4]);
    desc_len = unpack_u32be(&t->value.x[8 + mime_len]);
    pic_len  = unpack_u32be(&t->value.x[8 + mime_len + 4 + desc_len + 16]);

    src.mime.x = &t->value.x[8];
    src.mime.len = mime_len;

    src.desc.x = &t->value.x[8 + mime_len + 4];
    src.desc.len = desc_len;

    src.data.x = &t->value.x[8 + mime_len + 4 + desc_len + 20 ];
    src.data.len = pic_len;

    tmp_tag.key.x = (uint8_t*)"APIC";
    tmp_tag.key.len = 4;

    strbuf_init(&tmp_tag.value);

    r = m->picture_handler.cb(m->picture_handler.userdata, &src, &dest);
    if(r != 0) {
        taglist_shallow_free(&list);
        return r;
    }
    /* dest may or may not have data - if the mime length is empty,
     * that means dest just threw the data away and we should too */

    if(dest.mime.len > 0) {
        if( (r = membuf_ready(&tmp_tag.value, 32 + dest.mime.len + dest.desc.len + dest.data.len)) != 0) {
            taglist_shallow_free(&list);
            strbuf_free(&dest.mime);
            strbuf_free(&dest.desc);
            strbuf_free(&dest.data);
            return r;
        }
        /* note - need to make sure we free tmp_tag's value as well */
        memcpy(&tmp_tag.value.x[0],&t->value.x[0],4);
        pack_u32be(&tmp_tag.value.x[4],dest.mime.len);
        memcpy(&tmp_tag.value.x[8],dest.mime.x,dest.mime.len);
        pack_u32be(&tmp_tag.value.x[8 + dest.mime.len],dest.desc.len);
        memcpy(&tmp_tag.value.x[8 + dest.mime.len + 4],dest.desc.x,dest.desc.len);
        memcpy(&tmp_tag.value.x[8 + dest.mime.len + 4 + dest.desc.len],&t->value.x[8 + src.mime.len + 4 + src.desc.len],16);
        pack_u32be(&tmp_tag.value.x[8 + dest.mime.len + 4 + dest.desc.len + 16],dest.data.len);
        memcpy(&tmp_tag.value.x[8 + dest.mime.len + 4 + dest.desc.len + 20],dest.data.x,dest.data.len);
        tmp_tag.value.len = 32 + dest.mime.len + dest.desc.len + dest.data.len;
    }

    if( (r = taglist_add_tag(&list,&tmp_tag)) != 0) {
        taglist_shallow_free(&list);
        strbuf_free(&dest.mime);
        strbuf_free(&dest.desc);
        strbuf_free(&dest.data);
        strbuf_free(&tmp_tag.value);
    }

    r = m->plugin->submit_tags(m->userdata,&list);

    taglist_shallow_free(&list);
    strbuf_free(&dest.mime);
    strbuf_free(&dest.desc);
    strbuf_free(&dest.data);
    strbuf_free(&tmp_tag.value);

    return r;
}

int muxer_flush(const muxer* m) {
    return m->plugin->flush(m->userdata, &m->segment_handler);
}

