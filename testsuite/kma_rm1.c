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
	void* prev;
	void* next;
} blk_ptr_t;

typedef struct {
	void* this;
	blk_ptr_t* free_list;
	int allocated_block;
	int total_pages;
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
void coalesce(blk_ptr_t*);
void add_to_free_list(blk_ptr_t*, int);
void remove_from_free_list(blk_ptr_t*);
blk_ptr_t* find_first_fit(int);
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
	page_header->free_list = (blk_ptr_t*)((long int)page_header + sizeof(pg_hdr_t));//sizeof what?
	page_header->allocated_block = 0;
	page_header->total_pages = 0;
	add_to_free_list(page_header->free_list, PAGESIZE - sizeof(pg_hdr_t));
}

void add_to_free_list(blk_ptr_t* block, kma_size_t size) {
//	pg_hdr_t* current_page_header = (pg_hdr_t*)BASEADDR(block);
	pg_hdr_t* first_page_header = (pg_hdr_t*)(entry_page->ptr);
	blk_ptr_t* current = first_page_header->free_list;
	block->size = size;
	block->prev = NULL;
	//anything else? current==NULL and block==current???? contradiction???
	if (block == current) {
		block->next = NULL;
		return;
	}
	else if (block < current) {
		((blk_ptr_t*)(first_page_header->free_list))->prev = block;
		block->next = (blk_ptr_t*)(first_page_header->free_list);
		first_page_header->free_list = block;
	}
	else {
		while(current->next != NULL) {
			if (block < current) {
				break;
			}
			current = current->next; 
		}
/*		if (block < current) {
			blk_ptr_t* temp = current->prev;
			block->prev = temp;
			block->next = current;
			current->prev = block;
			temp->next = block;
		}
		else {
			current->next = block;
			block->prev = current;
		}*/
		blk_ptr_t* temp = current->next;
		current->next = block;
		block->next = temp;
		block->prev = current;
		if (temp != NULL) {
			temp->prev = block;
		}

	}
}

blk_ptr_t* find_first_fit(int size) {
	int min_size = sizeof(blk_ptr_t);
	if (size < sizeof(blk_ptr_t)) {
		size = min_size;
	}

	pg_hdr_t* first_page_header = (pg_hdr_t*)(entry_page->ptr);
	int cur_size;
	//for (i = 0; i <= num_of_pages; i++) {
		//printf("%d",i);
	//	pg_hdr_t* current_page = (pg_hdr_t*)((long int)first_page_header + i * PAGESIZE);
		//if (current_page == NULL) break;
	blk_ptr_t* current = first_page_header->free_list;
	while(current != NULL) {
		cur_size = current->size;
		if (cur_size >= size) {
			if (cur_size == size || (cur_size - size) < min_size) {
				remove_from_free_list(current);
				return current;
			}
			else {
				add_to_free_list((blk_ptr_t*)((long int)current + size), cur_size - size);
				remove_from_free_list(current);
				return current;
			}
		}
		current = current->next;
	}//while end
	//}//for end
	
	make_new_page();
	first_page_header->total_pages++;
	return find_first_fit(size);
	//return (blk_ptr_t*)((long int)first_page_header + sizeof(kma_page_t*) + (long int)(i * PAGESIZE));
}

void remove_from_free_list(blk_ptr_t* block) {
	//pg_hdr_t* current_page = (pg_hdr_t*)BASEADDR(block);
	blk_ptr_t* temp_next = block->next;
	blk_ptr_t* temp_prev = block->prev;
	//only one in the 
	if ((temp_prev) == NULL && (temp_next) == NULL) {
		pg_hdr_t* first_page_header = (pg_hdr_t*)entry_page->ptr;
		first_page_header->free_list = NULL;
		return;
	} 
	//delete head
	else if (block->prev == NULL) {
		pg_hdr_t* first_page_header = (pg_hdr_t*)entry_page->ptr;
		temp_next->prev = NULL;
		first_page_header->free_list = temp_next;
		return;
	}
	//delete tail
	else if (block->next == NULL) {
		temp_prev->next = NULL;
		return;
	}
	else {
		temp_prev->next = temp_next;
		temp_next->prev = temp_prev;
		return;
	}
}
 
void
kma_free(void* ptr, kma_size_t size)
{
	blk_ptr_t* block = (blk_ptr_t*)ptr;
	add_to_free_list(block, size);
	pg_hdr_t* current_page = (pg_hdr_t*)BASEADDR(block);
	//blk_ptr_t* current = (blk_ptr_t*)current_page->free_list;
	current_page->allocated_block--;
	//coalesce with its next one;
	//coalesce(block);
	//coalesce(block->prev);
	pg_hdr_t* first_page_header = (pg_hdr_t*)entry_page->ptr;
	pg_hdr_t* temp_page;
	int num_of_pages = first_page_header->total_pages;
	bool next_page_flag = TRUE;
	while (next_page_flag) {
		temp_page = (pg_hdr_t*)((long int)first_page_header + num_of_pages*PAGESIZE);
		blk_ptr_t* current = first_page_header->free_list;
		//
		next_page_flag = FALSE; 
		if (temp_page->allocated_block == 0) {
			while(current != NULL) {
				if ((pg_hdr_t*)BASEADDR(current) == temp_page) {
					remove_from_free_list(current);
				}
				current = current->next;
			}
			next_page_flag = TRUE;
			if (temp_page == first_page_header) {
				entry_page = NULL;
				next_page_flag = FALSE;
			}
			free_page((*temp_page).this);
			if (entry_page != NULL){
				first_page_header->total_pages--;
			} 
			num_of_pages--;
		}
	}
}

void coalesce(blk_ptr_t* block) {
	if (block == NULL) {
		return;
	}
	if (block->next != NULL) {
		blk_ptr_t* temp = block->next;
		if ((long int)block + block->size == (long int)temp) {
			block->size += temp->size;
			//block->next->prev = NULL;//need?
			block->next = temp->next;
		}
	}
}

#endif // KMA_RM