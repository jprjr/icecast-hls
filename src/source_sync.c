#include "source_sync.h"

void source_sync_init(source_sync* sync) {
    sync->dest = NULL;
    return;
}

void source_sync_free(source_sync* sync) {
    (void)sync;
    return;
}

int source_sync_frame(source_sync* sync, const frame* frame) {
    int r;

    /* before trying to send the frame make sure the status
     * flag is OK */
    if( (r = thread_atomic_int_load(&sync->dest->status)) != 0) return r;

    thread_atomic_ptr_store(&sync->dest->data, (void*)frame);
    thread_atomic_int_store(&sync->dest->type, (int)DESTINATION_SYNC_FRAME);
    thread_signal_raise(&sync->dest->ready);
    thread_signal_wait(&sync->dest->consumed, THREAD_SIGNAL_WAIT_INFINITE);
    return thread_atomic_int_load(&sync->dest->status);
}

int source_sync_tags(source_sync* sync, const taglist* tags) {
    int r;

    if( (r = thread_atomic_int_load(&sync->dest->status)) != 0) return r;

    thread_atomic_ptr_store(&sync->dest->data, (void*)tags);
    thread_atomic_int_store(&sync->dest->type, (int)DESTINATION_SYNC_TAGS);
    thread_signal_raise(&sync->dest->ready);
    thread_signal_wait(&sync->dest->consumed, THREAD_SIGNAL_WAIT_INFINITE);
    return thread_atomic_int_load(&sync->dest->status);
}

void source_sync_eof(source_sync* sync) {
    /* on EOF we don't wait for the destination threads to acknowledge */
    thread_atomic_ptr_store(&sync->dest->data, NULL);
    thread_atomic_int_store(&sync->dest->type, (int)DESTINATION_SYNC_EOF);
    thread_signal_raise(&sync->dest->ready);
}

