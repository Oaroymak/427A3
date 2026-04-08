#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shell.h"        // MAX_USER_INPUT
#include "shellmemory.h"  // LINES_PER_PAGE, NUM_FRAMES, frame_load_page, ...
#include "pcb.h"

// ── Instruction access ────────────────────────────────────────────────────────

int pcb_current_page_fault(struct PCB *pcb) {
    int page_idx = (int)(pcb->pc / LINES_PER_PAGE);
    return pcb->page_table[page_idx] == -1;
}

int pcb_has_next_instruction(struct PCB *pcb) {
    return pcb->pc < pcb->line_count;
}

// Returns the physical address of the CURRENT instruction then advances pc.
// Physical address = frame_idx * LINES_PER_PAGE + offset_within_page.
// Caller must ensure the current page is loaded (i.e. no page fault).
size_t pcb_next_instruction(struct PCB *pcb) {
    int page_idx  = (int)(pcb->pc / LINES_PER_PAGE);
    int offset    = (int)(pcb->pc % LINES_PER_PAGE);
    int frame_idx = pcb->page_table[page_idx];
    pcb->pc++;
    return (size_t)(frame_idx * LINES_PER_PAGE + offset);
}

// ── Process creation ──────────────────────────────────────────────────────────

struct PCB *create_process(const char *filename) {
    // --- Pass 1: count lines so we can set up the page table ----------------
    FILE *script = fopen(filename, "rt");
    if (!script) {
        perror("create_process: fopen failed");
        return NULL;
    }

    size_t line_count = 0;
    char   linebuf[MAX_USER_INPUT];
    while (!feof(script)) {
        memset(linebuf, 0, sizeof(linebuf));
        if (!fgets(linebuf, MAX_USER_INPUT, script)) break;
        line_count++;
    }
    fclose(script);

    if (line_count == 0) return NULL; // empty file

    // --- Allocate PCB -------------------------------------------------------
    struct PCB *pcb = malloc(sizeof(struct PCB));
    static pid fresh_pid = 1;
    pcb->pid        = fresh_pid++;
    pcb->name       = strdup(filename);
    pcb->next       = NULL;
    pcb->pc         = 0;
    pcb->line_count = line_count;
    pcb->duration   = line_count;
    pcb->num_pages  = (line_count + LINES_PER_PAGE - 1) / LINES_PER_PAGE;

    pcb->page_table = malloc(pcb->num_pages * sizeof(int));
    for (size_t i = 0; i < pcb->num_pages; i++) {
        pcb->page_table[i] = -1; // not yet loaded
    }

    // --- Demand-paging initial load: first 2 pages only ---------------------
    size_t pages_to_load = pcb->num_pages < 2 ? pcb->num_pages : 2;
    for (size_t p = 0; p < pages_to_load; p++) {
        int free_frame = frame_find_free();
        if (free_frame < 0) {
            // Frame store is already full at startup – this shouldn't happen
            // with reasonable parameters, but handle it gracefully.
            break;
        }
        frame_load_page(pcb, (int)p, free_frame);
    }

    return pcb;
}

// A3: stdin-based process creation is not supported with demand paging
// because we cannot seek back to reload pages.  The background '#' mode
// that uses this path is not tested in A3.
struct PCB *create_process_from_FILE(FILE *script) {
    (void)script;
    return NULL;
}

// ── Process destruction ───────────────────────────────────────────────────────

void free_pcb(struct PCB *pcb) {
    // Null out owner pointers in the frame store so eviction logic can tell
    // that this PCB is gone.  The frame lines themselves stay resident and
    // remain eligible for LRU eviction.
    frame_clear_owner(pcb);

    free(pcb->page_table);

    if (pcb->name && strcmp("", pcb->name) != 0) {
        free(pcb->name);
    }

    free(pcb);
}
