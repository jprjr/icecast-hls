#include "destination_sync.h"

#include <stddef.h>

void destination_sync_init(destination_sync* sync) {
    thread_atomic_int_store(&sync->type, DESTINATION_SYNC_UNKNOWN);
    thread_atomic_int_store(&sync->status, 0);
    thread_atomic_ptr_store(&sync->data, NULL);
    sync->on_tags.cb = NULL;
    sync->on_tags.userdata = NULL;
    sync->frame_receiver = frame_receiver_zero;
    sync->tagmap = NULL;
    sync->map_flags = NULL;

    thread_signal_init(&sync->ready);
    thread_signal_init(&sync->consumed);
}

void destination_sync_free(destination_sync* sync) {
    thread_signal_term(&sync->ready);
    thread_signal_term(&sync->consumed);
}

int destination_sync_run(destination_sync *sync) {
    int ret = -1;

    frame f;
    taglist tags;
    taglist id3_tags;
    destination_sync_type type;

    type = DESTINATION_SYNC_UNKNOWN;
    taglist_init(&tags);
    taglist_init(&id3_tags);
    frame_init(&f);

    for(;;) {
        thread_signal_wait(&sync->ready, THREAD_SIGNAL_WAIT_INFINITE);
        type = (destination_sync_type)thread_atomic_int_load(&sync->type);
        switch(type) {
            case DESTINATION_SYNC_QUIT: {
                ret = -2;
                goto cleanup;
            }
            case DESTINATION_SYNC_UNKNOWN: {
                ret = -1;
                goto cleanup;
            }
            case DESTINATION_SYNC_EOF: {
                ret = sync->frame_receiver.flush(sync->frame_receiver.handle);
                goto cleanup;
            }
            case DESTINATION_SYNC_FRAME: {
                /* TODO is making a deep copy needed? */
                if(frame_copy(&f,thread_atomic_ptr_load(&sync->data)) < 0) {
                    ret = -1;
                    goto cleanup;
                }

                thread_atomic_int_store(&sync->status,0);
                thread_signal_raise(&sync->consumed);

                if(sync->frame_receiver.submit_frame(sync->frame_receiver.handle,&f) < 0) {
                    ret = -1;
                    goto cleanup;
                }
                break;
            }
            case DESTINATION_SYNC_TAGS: {
                if(taglist_deep_copy(&tags,thread_atomic_ptr_load(&sync->data)) < 0) {
                    ret = -1;
                    goto cleanup;
                }

                thread_atomic_int_store(&sync->status,0);
                thread_signal_raise(&sync->consumed);

                if(taglist_map(sync->tagmap,&tags,sync->map_flags,&id3_tags) < 0) {
                    ret = -1;
                    goto cleanup;
                }

                if(sync->on_tags.cb(sync->on_tags.userdata,&id3_tags) < 0) {
                    ret = -1;
                    goto cleanup;
                }

                break;
            }
        }
    }

    cleanup:

    /* store our final status in case the source thread
     * is still trying to push to us */
    thread_atomic_int_store(&sync->status,ret);
    thread_signal_raise(&sync->consumed);

    frame_free(&f);
    taglist_free(&tags);
    taglist_free(&id3_tags);

    return ret;
}

