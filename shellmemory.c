#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "shell.h"        // MAX_USER_INPUT
#include "shellmemory.h"
#include "pcb.h"          // full definition of struct PCB (needed to access page_table)

// ── Variable store ────────────────────────────────────────────────────────────

struct memory_cell {
    char *var;
    char *value;
};

static struct memory_cell var_store[VAR_MEM_SIZE];

void mem_init(void) {
    for (int i = 0; i < VAR_MEM_SIZE; i++) {
        var_store[i].var   = "none";
        var_store[i].value = "none";
    }
    frame_store_init();
}

void mem_set_value(char *var_in, char *value_in) {
    // Update existing entry if found
    for (int i = 0; i < VAR_MEM_SIZE; i++) {
        if (strcmp(var_store[i].var, var_in) == 0) {
            var_store[i].value = strdup(value_in);
            return;
        }
    }
    // Allocate a new slot
    for (int i = 0; i < VAR_MEM_SIZE; i++) {
        if (strcmp(var_store[i].var, "none") == 0) {
            var_store[i].var   = strdup(var_in);
            var_store[i].value = strdup(value_in);
            return;
        }
    }
    // Variable store full – silently drop (matches A2 behaviour)
}

char *mem_get_value(char *var_in) {
    for (int i = 0; i < VAR_MEM_SIZE; i++) {
        if (strcmp(var_store[i].var, var_in) == 0) {
            return strdup(var_store[i].value);
        }
    }
    return NULL;
}

// ── Frame store ───────────────────────────────────────────────────────────────
//
// Physical memory is divided into NUM_FRAMES frames.
// Each frame stores LINES_PER_PAGE (=3) lines of code from a single page of
// one process.  A frame is "free" when num_lines == 0.
//
// LRU tracking: a monotonically-increasing global clock is incremented every
// time a frame is loaded or one of its lines is fetched via get_line().
// The frame with the smallest last_used timestamp is the LRU victim.

struct frame_entry {
    char   *lines[LINES_PER_PAGE]; // raw strings as read by fgets (include \n)
    int     num_lines;             // 0 = free; 1-3 = lines stored
    struct PCB *owner;             // NULL if free or owner process has exited
    int     page_idx;              // which page of the owner this frame holds
    size_t  last_used;             // LRU timestamp
};

static struct frame_entry frame_store[NUM_FRAMES];
static size_t lru_clock = 0;

// ── Internal helpers ──────────────────────────────────────────────────────────

static void frame_free_entry(int idx) {
    for (int j = 0; j < LINES_PER_PAGE; j++) {
        if (frame_store[idx].lines[j]) {
            free(frame_store[idx].lines[j]);
            frame_store[idx].lines[j] = NULL;
        }
    }
    frame_store[idx].num_lines = 0;
    frame_store[idx].owner     = NULL;
    frame_store[idx].page_idx  = -1;
    frame_store[idx].last_used = 0;
}

// ── Public frame-store API ────────────────────────────────────────────────────

void frame_store_init(void) {
    for (int i = 0; i < NUM_FRAMES; i++) {
        frame_free_entry(i);
    }
    lru_clock = 0;
}

int frame_find_free(void) {
    for (int i = 0; i < NUM_FRAMES; i++) {
        if (frame_store[i].num_lines == 0) return i;
    }
    return -1; // store is full
}

int frame_find_lru(void) {
    int    lru_idx = -1;
    size_t lru_ts  = (size_t)(-1); // maximum possible
    for (int i = 0; i < NUM_FRAMES; i++) {
        if (frame_store[i].num_lines > 0 && frame_store[i].last_used < lru_ts) {
            lru_ts  = frame_store[i].last_used;
            lru_idx = i;
        }
    }
    return lru_idx;
}

void frame_load_page(struct PCB *pcb, int page_idx, int frame_idx) {
    struct frame_entry *f = &frame_store[frame_idx];

    // Open the backing file and skip to the right page
    FILE *file = fopen(pcb->name, "rt");
    if (!file) {
        perror("frame_load_page: fopen failed");
        return;
    }

    char buf[MAX_USER_INPUT];
    int  skip = page_idx * LINES_PER_PAGE;
    for (int i = 0; i < skip; i++) {
        if (!fgets(buf, MAX_USER_INPUT, file)) break;
    }

    f->num_lines = 0;
    for (int i = 0; i < LINES_PER_PAGE; i++) {
        memset(buf, 0, sizeof(buf));
        if (!fgets(buf, MAX_USER_INPUT, file)) break;
        f->lines[i] = strdup(buf);
        f->num_lines++;
    }
    fclose(file);

    f->owner    = pcb;
    f->page_idx = page_idx;
    f->last_used = ++lru_clock;

    // Tell the PCB where its page now lives
    pcb->page_table[page_idx] = frame_idx;
}

void frame_evict(int frame_idx) {
    struct frame_entry *f = &frame_store[frame_idx];

    // If the owning PCB is still alive, mark the page as no longer resident
    if (f->owner != NULL) {
        f->owner->page_table[f->page_idx] = -1;
    }

    frame_free_entry(frame_idx);
}

void frame_clear_owner(struct PCB *pcb) {
    // Called when a PCB is freed so that frames belonging to dead processes
    // don't hold a dangling pointer.  The lines remain in the frame (they can
    // still be LRU-evicted) but we null out the owner reference.
    for (int i = 0; i < NUM_FRAMES; i++) {
        if (frame_store[i].owner == pcb) {
            frame_store[i].owner = NULL;
        }
    }
}

const char *frame_get_line(int frame_idx, int offset) {
    if (offset >= frame_store[frame_idx].num_lines) return NULL;
    return frame_store[frame_idx].lines[offset];
}

const char *get_line(size_t phys_addr) {
    int frame_idx = (int)(phys_addr / LINES_PER_PAGE);
    int offset    = (int)(phys_addr % LINES_PER_PAGE);

    assert(frame_idx < NUM_FRAMES);
    assert(frame_store[frame_idx].num_lines > offset);

    // Update LRU timestamp on every access
    frame_store[frame_idx].last_used = ++lru_clock;

    return frame_store[frame_idx].lines[offset];
}

// ── Legacy stubs ──────────────────────────────────────────────────────────────

void assert_linememory_is_empty(void) {
    // A3: no-op (frame store is reset by reset_linememory_allocator)
}

void reset_linememory_allocator(void) {
    // A3: clear the frame store so each exec starts with clean physical memory
    frame_store_init();
}
