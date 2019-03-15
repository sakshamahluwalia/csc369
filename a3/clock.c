#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"


extern int memsize;

extern int debug;

extern struct frame *coremap;

int clock_hand;

/* Page to evict is chosen using the clock algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */

int clock_evict() {
	while (1) {
		if (clock_hand >= memsize) {
			clock_hand = 0;
		}
		if (coremap[clock_hand].clock_reference == 1) {
			coremap[clock_hand].clock_reference = 0;
			clock_hand++;
		} else {
			return clock_hand;
		}
	}
	return 0;
}

/* This function is called on each access to a page to update any information
 * needed by the clock algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void clock_ref(pgtbl_entry_t *p) {
	coremap[p->frame >> PAGE_SHIFT].clock_reference = 1;
	return;
}

/* Initialize any data structures needed for this replacement
 * algorithm.
 */
void clock_init() {
	clock_hand = 0;
}