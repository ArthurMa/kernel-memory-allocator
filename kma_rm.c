/***************************************************************************
 *  Title: Kernel Memory Allocator
 * -------------------------------------------------------------------------
 *    Purpose: Kernel memory allocator based on the resource map algorithm
 *    Author: Stefan Birrer
 *    Copyright: 2004 Northwestern University
 ***************************************************************************/
/***************************************************************************
 *  ChangeLog:
 * -------------------------------------------------------------------------
 *    Revision 1.2  2009/10/31 21:28:52  jot836
 *    This is the current version of KMA project 3.
 *    It includes:
 *    - the most up-to-date handout (F'09)
 *    - updated skeleton including
 *        file-driven test harness,
 *        trace generator script,
 *        support for evaluating efficiency of algorithm (wasted memory),
 *        gnuplot support for plotting allocation and waste,
 *        set of traces for all students to use (including a makefile and README of the settings),
 *    - different version of the testsuite for use on the submission site, including:
 *        scoreboard Python scripts, which posts the top 5 scores on the course webpage
 *
 *    Revision 1.1  2005/10/24 16:07:09  sbirrer
 *    - skeleton
 *
 *    Revision 1.2  2004/11/05 15:45:56  sbirrer
 *    - added size as a parameter to kma_free
 *
 *    Revision 1.1  2004/11/03 23:04:03  sbirrer
 *    - initial version for the kernel memory allocator project
 *
 ***************************************************************************/
#ifdef KMA_RM
#define __KMA_IMPL__

/************System include***********************************************/
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

/************Private include**********************************************/
#include "kma_page.h"
#include "kma.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */

typedef struct {
	int size;
	blk_ptr_t* prev;
	blk_ptr_t* next;
} blk_ptr_t;

typedef struct {
//	kma_page_t* this;
	blk_ptr_t* free_list;
	int allocated_block;
} pg_hdr_t;

/*
typedef struct {
	int id;
	void* ptr;
	int size;
}
*/

/************Global Variables*********************************************/

static kma_page_t* entry_page = NULL;

/************Function Prototypes******************************************/
void make_new_page();
/************External Declaration*****************************************/

/**************Implementation***********************************************/

void*
kma_malloc(kma_size_t size)
{
	//
	if (size + sizeof(void*) > PAGESIZE) {
		return NULL;
	}

	if (entry_page == NULL) {
		make_new_page();
	}

	blk_ptr_t* block = find_first_fit(size);
	pg_hdr_t* current_page = (pg_hdr_t*)BASEADDR(block);
	current_page->allocated_block++;

	return block;
}

void make_new_page() {
	kma_page_t* new_page = get_page();

	if (entry_page == NULL) {
		entry_page = new_page;
	}
	// add a pointer to the page structure at the beginning of the page
	*((kma_page_t**)(new_page->ptr)) = new_page;
	//page_header point to page space
	pg_hdr_t* page_header = (pg_hdr_t*)(new_page->ptr);
	page_header->free_list = (blk_ptr_t*)((long int)page_header + sizeof(kma_page_t*));//sizeof what?
	page_header->allocated_block = 0;
	add_to_free_list(page_header->free_list, PAGESIZE - sizeof(kma_page_t*));
}

void add_to_free_list(blk_ptr_t* block, kma_size_t size) {
	pg_hdr_t* current_page_header = (pg_hdr_t*)BASEADDR(block);
	blk_ptr_t* current = current_page_header->free_list;
	block->size = size;
	block->prev = NULL;
	block->next = NULL;
	if (block == current) {
		block->next = NULL;
	}
	else if (block > current) {
		while(current->next != NULL) {
			if (block < current) {
				break;
			}
			current = current->next; 
		}
		if (block < current) {
			block->prev = current->prev
			block->next = current;
			current->prev = block;
			block->prev->next = block;
			break;
		}
		else {
			current->next = block;
			block->prev = current;
		}
	}
}

blk_ptr_t* find_first_fit(kma_size_t size) {
	int min_size = sizeof(blk_ptr_t);
	if (size < sizeof(blk_ptr_t)) {
		size = 
	}
}


void
kma_free(void* ptr, kma_size_t size)
{

}


#endif // KMA_RM