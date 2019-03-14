#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"


extern int memsize;

extern int debug;

extern struct frame *coremap;

int myIndex;

/* Page to evict is chosen using the fifo algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */
int fifo_evict() {

	// use linked list evict head


	int min = myIndex;
	int pos = 0;

	for (int i = 0; i < memsize; i++) {

		if (min > coremap[i].entry) {
			min = coremap[i].entry;
			pos = i;
		}
	}

	return pos;
}

/* This function is called on each access to a page to update any information
 * needed by the fifo algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void fifo_ref(pgtbl_entry_t *p) {

	// add to the tail by keeping and updating a reference to it.

	if (coremap[p->frame >> PAGE_SHIFT].new_page) {
		
		coremap[p->frame >> PAGE_SHIFT].new_page = 0;
		coremap[p->frame >> PAGE_SHIFT].entry = myIndex++;

	}
	return;
}

/* Initialize any data structures needed for this 
 * replacement algorithm 
 */
void fifo_init() {

	for (int i = 0; i < memsize; i++)
	{
		coremap[i].new_page = 0;
		coremap[i].entry = 0;
	}

	myIndex = 0;

}