#pragma once
#include <stddef.h>
#include <stdio.h>

typedef size_t pid;

struct PCB {
    pid     pid;
    char   *name;
    // A3: line_base removed (pages are no longer stored contiguously).
    // line_count is still the total number of lines in the program.
    size_t  line_count;
    size_t  duration;
    size_t  pc;           // logical program counter (0 .. line_count-1)
    struct PCB *next;

    // A3: demand paging
    int    *page_table;   // page_table[i] = frame index, or -1 if not in memory
    size_t  num_pages;    // ceil(line_count / LINES_PER_PAGE)
};

// Returns non-zero if the current page (pc / LINES_PER_PAGE) is not loaded.
int    pcb_current_page_fault(struct PCB *pcb);

// Returns non-zero while there are more instructions (pc < line_count).
int    pcb_has_next_instruction(struct PCB *pcb);

// Returns the physical address of the current instruction and advances pc.
// MUST only be called when pcb_current_page_fault() returns 0.
size_t pcb_next_instruction(struct PCB *pcb);

struct PCB *create_process(const char *filename);
struct PCB *create_process_from_FILE(FILE *f); // A3: returns NULL (unsupported)
void        free_pcb(struct PCB *pcb);
