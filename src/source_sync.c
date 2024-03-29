#include "source_sync.h"

void source_sync_init(source_sync* sync) {
    sync->dest = NULL;
    return;
}

void source_sync_free(source_sync* sync) {
    (void)sync;
    return;
}

int source_sync_open(source_sync* sync, const frame_source* source) {
    int r;

    if( (r = thread_atomic_int_load(&sync->dest->status)) != 0) return r;

    thread_atomic_ptr_store(&sync->dest->data, (void*)source);
    thread_atomic_int_store(&sync->dest->type, (int)DESTINATION_SYNC_OPEN);
    thread_signal_raise(&sync->dest->ready);
    thread_signal_wait(&sync->dest->consumed, THREAD_SIGNAL_WAIT_INFINITE);
    return thread_atomic_int_load(&sync->dest->status);
}

int source_sync_frame(source_sync* sync, const frame* frame) {
    int r;

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

int source_sync_flush(source_sync* sync) {
    int r;

    if( (r = thread_atomic_int_load(&sync->dest->status)) != 0) return r;

    thread_atomic_ptr_store(&sync->dest->data, NULL);
    thread_atomic_int_store(&sync->dest->type, (int)DESTINATION_SYNC_FLUSH);
    thread_signal_raise(&sync->dest->ready);
    thread_signal_wait(&sync->dest->consumed, THREAD_SIGNAL_WAIT_INFINITE);
    return thread_atomic_int_load(&sync->dest->status);
}

int source_sync_reset(source_sync* sync) {
    int r;

    if( (r = thread_atomic_int_load(&sync->dest->status)) != 0) return r;

    thread_atomic_ptr_store(&sync->dest->data, NULL);
    thread_atomic_int_store(&sync->dest->type, (int)DESTINATION_SYNC_RESET);
    thread_signal_raise(&sync->dest->ready);
    thread_signal_wait(&sync->dest->consumed, THREAD_SIGNAL_WAIT_INFINITE);
    return thread_atomic_int_load(&sync->dest->status);
}

int source_sync_eof(source_sync* sync) {
    int r;

    if( (r = thread_atomic_int_load(&sync->dest->status)) != 0) return r;
    thread_atomic_ptr_store(&sync->dest->data, NULL);
    thread_atomic_int_store(&sync->dest->type, (int)DESTINATION_SYNC_EOF);
    thread_signal_raise(&sync->dest->ready);
    thread_signal_wait(&sync->dest->consumed, THREAD_SIGNAL_WAIT_INFINITE);
    return thread_atomic_int_load(&sync->dest->status);
}

/* in case of some kind of "you gotta quit right now emergency" */
void source_sync_quit(source_sync* sync) {
    /* on EOF we don't wait for the destination threads to acknowledge */
    thread_atomic_ptr_store(&sync->dest->data, NULL);
    thread_atomic_int_store(&sync->dest->type, (int)DESTINATION_SYNC_QUIT);
    thread_signal_raise(&sync->dest->ready);
}
