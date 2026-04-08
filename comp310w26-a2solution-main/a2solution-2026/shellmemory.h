#pragma once
#include <stdio.h>
#include <stddef.h>

// A3: Compile-time memory sizes (overridden by -D flags from the Makefile)
#ifndef FRAME_STORE_SIZE
#define FRAME_STORE_SIZE 18
#endif

#ifndef VAR_MEM_SIZE
#define VAR_MEM_SIZE 10
#endif

// Each page/frame holds exactly 3 lines of code
#define LINES_PER_PAGE 3
#define NUM_FRAMES (FRAME_STORE_SIZE / LINES_PER_PAGE)

// Forward declaration so frame store API can reference PCB without a
// circular include (pcb.h already includes shellmemory.h indirectly via pcb.c)
struct PCB;

// ── Variable store (shell variables: set / print) ────────────────────────────
void  mem_init(void);
char *mem_get_value(char *var);
void  mem_set_value(char *var, char *value);

// ── Frame store (physical memory for program pages) ──────────────────────────

// Reset all frames to free (called at the start of every exec).
void frame_store_init(void);

// Return the index of a free frame, or -1 if the store is full.
int  frame_find_free(void);

// Return the index of the least-recently-used in-use frame.
int  frame_find_lru(void);

// Load page page_idx of pcb from its backing file into frame_idx.
// Updates pcb->page_table[page_idx] and the frame's LRU timestamp.
void frame_load_page(struct PCB *pcb, int page_idx, int frame_idx);

// Evict frame_idx: free its line strings, set its owner's page_table entry
// to -1 (if the owner PCB is still alive), and mark the frame as free.
// Does NOT print any message – that is the caller's responsibility.
void frame_evict(int frame_idx);

// When a PCB is freed, null out the owner pointer in every frame it owns
// so dangling-pointer evictions are avoided.
void frame_clear_owner(struct PCB *pcb);

// Return the raw line string at a given offset inside a frame (0-based).
// Returns NULL if the offset is beyond the number of lines stored there
// (e.g. the last page of a file has fewer than 3 lines).
const char *frame_get_line(int frame_idx, int offset);

// Fetch the line at physical address phys_addr = frame_idx*3 + offset.
// Updates the LRU timestamp of the owning frame.
// This is what interpreter.c calls instead of the old get_line(linememory index).
const char *get_line(size_t phys_addr);

// ── Legacy stubs (kept so existing call-sites in interpreter.c compile) ──────
// reset_linememory_allocator is called by my_exec before each scheduling run;
// in A3 it simply calls frame_store_init().
void assert_linememory_is_empty(void);
void reset_linememory_allocator(void);
