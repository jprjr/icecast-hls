#include "muxer.h"
#include "unpack_u32be.h"
#include "pack_u32be.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int muxer_default_picture_handler(void* userdata, const picture* src, picture* out) {
    (void)userdata;
    (void)src;
    (void)out;
    fprintf(stderr,"[muxer] picture handler not set\n");
    return -1;
}

void muxer_init(muxer* m) {
    m->userdata = NULL;
    m->plugin = NULL;
    m->segment_receiver = segment_receiver_zero;
    m->picture_handler.cb = muxer_default_picture_handler;
    m->picture_handler.userdata = NULL;
    m->image_mode = 0;
    m->output_opened = 1;
}

void muxer_free(muxer* m) {
    if(m->userdata != NULL) {
        m->plugin->close(m->userdata);
        free(m->userdata);
    }
    m->userdata = NULL;
    m->plugin = NULL;
}

int muxer_create(muxer* m, const strbuf* name) {
    const muxer_plugin* plug;
    void* userdata;

    plug = muxer_plugin_get(name);
    if(plug == NULL) return -1;

    userdata = malloc(plug->size());
    if(userdata == NULL) return -1;

    m->userdata = userdata;
    m->plugin = plug;

    return m->plugin->create(m->userdata);
}

int muxer_reset(const muxer* m) {
    return m->plugin->reset(m->userdata);
}

/* most other things _open method (filter_open, encoder_open, etc) call
 * flush + reset on a re-open. But muxers do NOT flush + reset the output,
 * the only that that should call flush on an output is the destination
 * object, since flushing an output triggers end-of-stream type behavior
 * (close out the playlist, etc). Instead the muxer just sends a new
 * initialization segment. */

static int muxer_open_wrapper(void* ud, const segment_source* source) {
    muxer* m = (muxer *)ud;

    if(m->output_opened == 0) return m->segment_receiver.reset(m->segment_receiver.handle); /* output already opened, no need to re-open */

    m->output_opened = m->segment_receiver.open(m->segment_receiver.handle, source);
    return m->output_opened;
}

static int muxer_get_segment_info_wrapper(const void* ud, const segment_source_info* i, segment_params* p) {
    const muxer* m = (muxer *)ud;
    return m->segment_receiver.get_segment_info(m->segment_receiver.handle,i,p);
}

static int muxer_submit_segment_wrapper(void* ud, const segment* s) {
    const muxer* m = (muxer *)ud;
    return m->segment_receiver.submit_segment(m->segment_receiver.handle,s);
}

static int muxer_submit_tags_wrapper(void* ud, const taglist* s) {
    const muxer* m = (muxer *)ud;
    return m->segment_receiver.submit_tags(m->segment_receiver.handle,s);
}

static int muxer_flush_wrapper(void* ud) {
    const muxer* m = (muxer *)ud;
    return m->segment_receiver.flush(m->segment_receiver.handle);
}

int muxer_open(muxer* m, const packet_source* source) {
    segment_receiver receiver = SEGMENT_RECEIVER_ZERO;

    if(m->plugin == NULL || m->userdata == NULL) {
        fprintf(stderr,"[muxer] unable to open: plugin not selected\n");
        return -1;
    }
    ich_time_now(&m->ts);
    m->counter = 0;

    receiver.handle = m;
    receiver.open = muxer_open_wrapper;
    receiver.get_segment_info = muxer_get_segment_info_wrapper;
    receiver.submit_segment = muxer_submit_segment_wrapper;
    receiver.submit_tags = muxer_submit_tags_wrapper;
    receiver.flush = muxer_flush_wrapper;

    return m->plugin->open(m->userdata, source, &receiver);
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

int muxer_submit_packet(muxer* m, const packet* p) {
    int r = m->plugin->submit_packet(m->userdata, p, &m->segment_receiver);
    if(r == 0) {
        ich_time_now(&m->ts);
        m->counter++;
    }
    return r;
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

    apic_idx = taglist_find_cstr(tags,"APIC",0);
    /* if there's no APIC there's nothing to do */
    if(apic_idx == taglist_len(tags)) {
        return m->plugin->submit_tags(m->userdata, tags, &m->segment_receiver);
    }

    /* if we're keeping and keeping in-band, nothing to do */
    if( (m->image_mode & IMAGE_MODE_KEEP) && (m->image_mode & IMAGE_MODE_INBAND)) {
        return m->plugin->submit_tags(m->userdata, tags, &m->segment_receiver);
    }

    taglist_init(&list);
    if(taglist_shallow_copy(&list,tags) != 0) return -1;
    /* NOTE list has to be freed now! */

    taglist_remove_tag(&list,apic_idx);

    if(! (m->image_mode & IMAGE_MODE_KEEP)) {
        r = m->plugin->submit_tags(m->userdata,&list, &m->segment_receiver);
        taglist_shallow_free(&list);
        return r;
    }

    /* (maybe) move image out-of-band */
    t = taglist_get_tag(tags,apic_idx);

    /* let's get our meta-info from the picture */
    mime_len = unpack_u32be(&t->value.x[4]);
    desc_len = unpack_u32be(&t->value.x[8 + mime_len]);
    pic_len  = unpack_u32be(&t->value.x[8 + mime_len + 4 + desc_len + 16]);

    src.mime.x = &t->value.x[8];
    src.mime.len = mime_len;

    if(strbuf_equals_cstr(&src.mime, "-->")) {
        /* image is already a link! */
        taglist_shallow_free(&list);
        return m->plugin->submit_tags(m->userdata, tags, &m->segment_receiver);
    }

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
        if(dest.desc.len) memcpy(&tmp_tag.value.x[8 + dest.mime.len + 4],dest.desc.x,dest.desc.len);
        memcpy(&tmp_tag.value.x[8 + dest.mime.len + 4 + dest.desc.len],&t->value.x[8 + src.mime.len + 4 + src.desc.len],16);
        pack_u32be(&tmp_tag.value.x[8 + dest.mime.len + 4 + dest.desc.len + 16],dest.data.len);
        memcpy(&tmp_tag.value.x[8 + dest.mime.len + 4 + dest.desc.len + 20],dest.data.x,dest.data.len);
        tmp_tag.value.len = 32 + dest.mime.len + dest.desc.len + dest.data.len;
    }

    if( (r = taglist_add_tag(&list,&tmp_tag)) != 0) goto cleanup;
    r = m->plugin->submit_tags(m->userdata,&list, &m->segment_receiver);

    cleanup:

    taglist_shallow_free(&list);
    strbuf_free(&dest.mime);
    strbuf_free(&dest.desc);
    strbuf_free(&dest.data);
    strbuf_free(&tmp_tag.value);

    return r;
}

int muxer_flush(const muxer* m) {
    return m->plugin->flush(m->userdata, &m->segment_receiver);
}

uint32_t muxer_get_caps(const muxer* m) {
    return m->plugin->get_caps(m->userdata);
}

int muxer_get_segment_info(const muxer* m, const packet_source_info* s, packet_source_params* i) {
    return m->plugin->get_segment_info(m->userdata,s,&m->segment_receiver,i);
}

void muxer_dump_counters(const muxer* in, const strbuf* prefix) {
    ich_tm tm;
    ich_time_to_tm(&tm,&in->ts);

    fprintf(stderr,"%.*s muxer: muxes=%zu last_mux=%4u-%02u-%02u %02u:%02u:%02u\n",
      (int)prefix->len,(const char*)prefix->x,
      in->counter,
      tm.year,tm.month,tm.day,
      tm.hour,tm.min,tm.sec);
}
