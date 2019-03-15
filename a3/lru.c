#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"


extern int memsize;

extern int debug;

extern struct frame *coremap;

int size;
node_t* head;
node_t* tail;

/* Page to evict is chosen using the accurate LRU algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */

int lru_evict() {
	int frame;
	frame = head->val;
	head = head->right;
	printf("DEBUG\n");
	size--;
	return frame;
}

/* This function is called on each access to a page to update any information
 * needed by the lru algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void lru_ref(pgtbl_entry_t *p) {

	if (coremap[p->frame >> PAGE_SHIFT].new_page) { // miss page not in mem
		
		coremap[p->frame >> PAGE_SHIFT].page_node->val = p->frame >> PAGE_SHIFT;

		if (size == 0)  { // no entries in linked list

			coremap[p->frame >> PAGE_SHIFT].page_node->left = NULL;
			coremap[p->frame >> PAGE_SHIFT].page_node->right = NULL;

			head = coremap[p->frame >> PAGE_SHIFT].page_node;
			tail = head;

		} else { // 1 or more entries

			coremap[p->frame >> PAGE_SHIFT].page_node->left = tail;
			coremap[p->frame >> PAGE_SHIFT].page_node->right = NULL;
			tail->right = coremap[p->frame >> PAGE_SHIFT].page_node;
			tail = coremap[p->frame >> PAGE_SHIFT].page_node;

		}

		size++;
		coremap[p->frame >> PAGE_SHIFT].new_page = 0;

	} else { // hit page in mem

		if (coremap[p->frame >> PAGE_SHIFT].page_node != tail) {
			
			if (coremap[p->frame >> PAGE_SHIFT].page_node == head) {

				head = head->right;
				head->left = NULL;
				tail->right = coremap[p->frame >> PAGE_SHIFT].page_node;
				coremap[p->frame >> PAGE_SHIFT].page_node->left = tail;
				coremap[p->frame >> PAGE_SHIFT].page_node->right = NULL;
				tail = coremap[p->frame >> PAGE_SHIFT].page_node;

			} else {

				coremap[p->frame >> PAGE_SHIFT].page_node->left->right = coremap[p->frame >> PAGE_SHIFT].page_node->right;
				coremap[p->frame >> PAGE_SHIFT].page_node->right->left = coremap[p->frame >> PAGE_SHIFT].page_node->left;
				tail->right = coremap[p->frame >> PAGE_SHIFT].page_node;
				coremap[p->frame >> PAGE_SHIFT].page_node->left = tail;
				coremap[p->frame >> PAGE_SHIFT].page_node->right = NULL;
				tail = coremap[p->frame >> PAGE_SHIFT].page_node;

			}

		}

	}

}


/* Initialize any data structures needed for this 
 * replacement algorithm 
 */
void lru_init() {


	head = malloc(sizeof(node_t));
	head->left = malloc(sizeof(node_t));
	head->right = malloc(sizeof(node_t));

	tail = malloc(sizeof(node_t));
	tail->left = malloc(sizeof(node_t));
	tail->right = malloc(sizeof(node_t));

	for (int i = 0; i < memsize; ++i)
	{
		coremap[i].page_node = malloc(sizeof(node_t));
		coremap[i].page_node->left = malloc(sizeof(node_t));
		coremap[i].page_node->right = malloc(sizeof(node_t));
	}

	size = 0;

}