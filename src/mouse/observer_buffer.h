#ifndef HOGS_OBSERVER_BUFFER_H
#define HOGS_OBSERVER_BUFFER_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

enum {
    HOGS_OBSERVER_BATCH_ROWS = 64,
    HOGS_OBSERVER_ROW_BYTES = 512,
    HOGS_OBSERVER_BATCH_BYTES = HOGS_OBSERVER_BATCH_ROWS * HOGS_OBSERVER_ROW_BYTES
};

typedef struct {
    char bytes[HOGS_OBSERVER_BATCH_BYTES];
    size_t used;
    uint32_t rows;
} HogsObserverBatch;

typedef int (*HogsObserverSink)(void *context, const char *bytes, size_t count);

/* Compactly append a complete CSV row. The terminating NUL is excluded from
   count; embedded NULs are rejected so the exact byte stream stays CSV-safe. */
static inline int hogs_observer_batch_append(HogsObserverBatch *batch,
                                             const char *row, size_t count) {
    if (!batch || !row || !count || count >= HOGS_OBSERVER_ROW_BYTES ||
        batch->rows >= HOGS_OBSERVER_BATCH_ROWS ||
        count > HOGS_OBSERVER_BATCH_BYTES - batch->used ||
        memchr(row, '\0', count)) return 0;
    memcpy(batch->bytes + batch->used, row, count);
    batch->used += count;
    batch->rows++;
    return 1;
}

/* On sink failure, retain the batch for explicit caller handling. */
static inline int hogs_observer_batch_drain(HogsObserverBatch *batch,
                                            HogsObserverSink sink,
                                            void *context) {
    if (!batch || !sink) return 0;
    if (!batch->rows && !batch->used) return 1;
    if (!batch->rows || !batch->used || !sink(context, batch->bytes, batch->used)) return 0;
    batch->used = 0;
    batch->rows = 0;
    return 1;
}

static inline void hogs_observer_batch_clear(HogsObserverBatch *batch) {
    if (!batch) return;
    batch->used = 0;
    batch->rows = 0;
}

static inline int hogs_observer_make_filename(char *out, size_t capacity,
                                              uint32_t pid, uint32_t tick,
                                              uint32_t sequence) {
    if (!out || !capacity) return -1;
    int n = snprintf(out, capacity, "HogsMouseObserver-%08lx-%08lx-%02lx.csv",
                     (unsigned long)pid, (unsigned long)tick,
                     (unsigned long)sequence);
    if (n < 0 || (size_t)n >= capacity) return -1;
    return n;
}

#endif
