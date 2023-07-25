#ifndef DESTINATION_SYNC_H
#define DESTINATION_SYNC_H

/* this is the core meeting-point between a source
 * thread and a destination thread.
 *
 * A destination thread will (roughly):
 *   * wait on a ready signal
 *   * check the incoming datatype, which can be:
 *     an "open" command
 *     a frame reference
 *     a taglist reference
 *     a "flush" command
 *     an EOF marker
 *   * immediately copy the data into local memory
 *   * set the "everything-is-ok" flag
 *   * raise the consumed signal
 *
 * A source thread will (roughly)
 *   * set the incoming data type
 *   * add a reference to the appropriate datatype
 *   * raise the ready signal
 *   * wait on the consumed signal
 *   * check the "everythng is OK" flag
 *
 * It's imperative that the destination thread set the status
 * flag and raise the consumed signal to keep the source thread
 * from waiting forever.
 */

#include "thread.h"
#include "frame.h"
#include "tag.h"

enum destination_sync_type {
    DESTINATION_SYNC_QUIT    = -2,
    DESTINATION_SYNC_UNKNOWN = -1,
    DESTINATION_SYNC_EOF     =  0,
    DESTINATION_SYNC_OPEN    =  1,
    DESTINATION_SYNC_FRAME   =  2,
    DESTINATION_SYNC_TAGS    =  3,
    DESTINATION_SYNC_FLUSH   =  4,
    DESTINATION_SYNC_RESET   =  5,
};

typedef enum destination_sync_type destination_sync_type;

struct destination_sync {
    thread_atomic_int_t type;
    thread_atomic_int_t status;
    thread_signal_t ready;
    thread_signal_t consumed;
    thread_atomic_ptr_t data;
    tag_handler on_tags;
    frame_receiver frame_receiver;
    const taglist* tagmap;
    const taglist_map_flags* map_flags;
};

typedef struct destination_sync destination_sync;

#ifdef __cplusplus
extern "C" {
#endif

void destination_sync_init(destination_sync*);
void destination_sync_free(destination_sync*);

/* the thread's main function */
int destination_sync_run(destination_sync*);

#ifdef __cplusplus
}
#endif

#endif
