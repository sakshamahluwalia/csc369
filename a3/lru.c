#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"


extern int memsize;

extern int debug;

extern struct frame *coremap;

typedef struct node {
    int val;
    struct node* right;
    struct node* left;
} node_t;

node_t* head;
int size;





/* Page to evict is chosen using the accurate LRU algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */

int lru_evict() {
	
	// pop head make head.next = head
	int frame = head->val;
	head = head->right;
	if (head->left != NULL)
	{
		head->left = NULL;
	}
	return frame;
}

/* This function is called on each access to a page to update any information
 * needed by the lru algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void lru_ref(pgtbl_entry_t *p) {

	// update linked list
	if (coremap[p->frame >> PAGE_SHIFT].new_page) {

		node_t* node = malloc(sizeof(node_t));
		
		// miss (not in linked list)
		if (size == 0) {

			node->val = p->frame >> PAGE_SHIFT;
			node->right = NULL;
			node->left = NULL;

			head = node;

		} else {

			// linked list has items in it.
			// walk the list and find tail.
			node_t* tail = head;
			while (tail->right != NULL) {
				tail = tail->right;
			}

			assert(tail->right == NULL);
			// node is the tail

			// create a new node
			node->val = p->frame >> PAGE_SHIFT;
			node->right = NULL;

			// make node the tail and update the old tail pointer
			node->left = tail;
			tail->right = node;
		}
		size++;

	} else {

		// hit (already somewhere in the linked list)
		// find it
		node_t* curr = head;
		while (curr->right != NULL) {

			if (curr->val == p->frame >> PAGE_SHIFT) {
				break;
			}

			curr = curr->right;
		}

		// disconnect curr node
		curr->left->right = curr->right;
		curr->right->left = curr->left;

		// curr is the node containing the page frame.
		assert(curr->val == p->frame >> PAGE_SHIFT);

		// move this node to the end.
		node_t* node = head;
		while (node->right != NULL) {
			node = node->right;
		}

		// update the [pointers]
		node->right = curr;
		curr->left = node;
		curr->right = NULL;
	}
}


/* Initialize any data structures needed for this 
 * replacement algorithm 
 */
void lru_init() {

	// initialise linked list with head and tail
	head = malloc(sizeof(node_t));
	head->left = malloc(sizeof(node_t));
	head->right = malloc(sizeof(node_t));
	head->left = NULL;
	size = 0;
}