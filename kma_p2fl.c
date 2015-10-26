/***************************************************************************
 *  Title: Kernel Memory Allocator
 * -------------------------------------------------------------------------
 *    Purpose: Kernel memory allocator based on the power-of-two free list
 *             algorithm
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
 *    Revision 1.3  2004/12/03 23:04:03  sbirrer
 *    - initial version for the kernel memory allocator project
 *
 
 ************************************************************************/

/************************************************************************
 Project Group: NetID1, NetID2, NetID3
 
 ***************************************************************************/

#ifdef KMA_P2FL
#define __KMA_IMPL__

/************System include***********************************************/
#include <assert.h>
#include <stdlib.h>

/************Private include**********************************************/
#include "kma_page.h"
#include "kma.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */
#define MINPOWER 4
#define MINSIZE 16
#define HDRSIZE 10

typedef struct blk_ptr{
  struct blk_ptr* next;
} blk_ptr_t;

typedef struct pg_hdr{
  kma_page_t* this;
  struct pg_hdr* prev;
  struct pg_hdr* next;
  int free_size;
} pg_hdr_t;

typedef struct {
  int size;
  blk_ptr_t* next;
} bf_lst_t;

typedef struct {
  int allocated;
  int freed;
  bf_lst_t free_list[HDRSIZE];
  pg_hdr_t* page_list;
} mem_ctrl_t;

/************Global Variables*********************************************/
static kma_page_t* entry_page = NULL;
/************Function Prototypes******************************************/
mem_ctrl_t* pg_master();
int next_power_of_two(int);
void* kma_malloc(kma_size_t);
void kma_free(void*, kma_size_t);
void* find_fit(kma_size_t);
void init_page();
void* get_new_free_block(kma_size_t);
void add_to_free_list(void*, int);
void free_all();
void delete_node(int i,mem_ctrl_t* controller);
/************External Declaration*****************************************/

/**************Implementation***********************************************/
//Done
mem_ctrl_t* pg_master(){
  return (mem_ctrl_t*)((void*)entry_page->ptr + sizeof(kma_page_t*));
}
//Done
int next_power_of_two(int n) {
  int p = 1;
  if (n && !(n & (n-1)))
    return n;

  while (p < n) {
    p <<= 1;
  }
  return p;
}
//---------KMA_MALLOC-----------//
//need to consider extra size
void* kma_malloc(kma_size_t size) {
  if (size + sizeof(void*) > PAGESIZE)  
    return NULL;

  if (entry_page == NULL)
    init_page();
  //change the size
  size += sizeof(blk_ptr_t);
  if (size < MINSIZE)
    size = MINSIZE;

  mem_ctrl_t* controller = pg_master();
  void* block = find_fit(size);
  controller->allocated++;

  return block;
}

//Done
void init_page() {
  kma_page_t* new_page = get_page();
  entry_page = new_page;
  *((kma_page_t**)new_page->ptr) = new_page;

  mem_ctrl_t* controller = pg_master();
  
  controller->page_list = (pg_hdr_t*)((void*)controller + sizeof(mem_ctrl_t));
  controller->page_list->this = (kma_page_t*)new_page->ptr;
  controller->page_list->prev = NULL;
  controller->page_list->next = NULL;
  controller->page_list->free_size = PAGESIZE - sizeof(kma_page_t*) - sizeof(mem_ctrl_t) - sizeof(pg_hdr_t);
  int i;
  for (i = 0; i < HDRSIZE; i++) {
    controller->free_list[i].size = 1 << (i + MINPOWER);
    controller->free_list[i].next = NULL;
  } 
  controller->allocated = 0;
  controller->freed = 0;
}
//Done!!
int get_index(int n) {
  n = next_power_of_two(n);
  int count = 0;
  while(n) {
    count++;
    n >>= 1;
  }
  return count - MINPOWER - 1;
}
//need to consider extra size
void* find_fit(kma_size_t size) {
  mem_ctrl_t* controller = pg_master();

  int ind = get_index(size);
  void* blk = NULL;
  bf_lst_t lst = controller->free_list[ind];
  if (lst.next) {
    blk = (void*)lst.next;
    controller->free_list[ind].next = controller->free_list[ind].next->next;
  }
  else {
    blk = get_new_free_block(size);
  }

  return blk;
}
//Done
void* get_new_free_block(kma_size_t size) {
  if (size <= 4096) {
    size = next_power_of_two(size);
  }
  mem_ctrl_t* controller = pg_master();
  pg_hdr_t* current_page = controller->page_list;

  while (current_page) {
    if (size <= 4096 && current_page->free_size > size) {
      current_page->free_size = current_page->free_size - size;
      return (void*)((void*)current_page->this + (PAGESIZE - current_page->free_size) - size);
    }
    else 
      current_page = current_page->next;
  }

  kma_page_t* new_page = get_page();
  *((kma_page_t**)new_page->ptr) = new_page;
  pg_hdr_t* current = (pg_hdr_t*)((void*)new_page->ptr + sizeof(kma_page_t*));
  current->this = (kma_page_t*)(new_page->ptr);
  current->next = NULL;
  current->free_size = PAGESIZE - sizeof(kma_page_t*) - sizeof(pg_hdr_t);

  pg_hdr_t* previous = controller->page_list;
  while (previous) {
    if (previous->next == NULL) {
      current->prev = previous;
      previous->next = current;
      break;
    }
    else
      previous = previous->next;
  }

  if (size > 4096) {
    current->free_size = 0;
    return (void*)((void*)current + sizeof(pg_hdr_t));
  }
  else {
    current->free_size -= size;
    return (void*)((void*)new_page->ptr + (PAGESIZE - current->free_size) - size); 
  }
}
//Done
void add_to_free_list(void* ptr, int size) {
  mem_ctrl_t* controller = pg_master();
  int ind = get_index(size);
  ((blk_ptr_t*)ptr)->next = controller->free_list[ind].next;
  controller->free_list[ind].next = (blk_ptr_t*)ptr;
  return;
}
//need to consider a lot what is the size to be
//what is the size? 1. the original size 2. the malloc size
void kma_free(void* ptr, kma_size_t size)
{ 
  size += sizeof(blk_ptr_t);
  if (size < MINSIZE) 
    size = MINSIZE;

  add_to_free_list(ptr, size);
  mem_ctrl_t* controller = pg_master();
  controller->freed++;
  if (controller->freed == controller->allocated)
    free_all();
  return;
}
//Done
void free_all() {
  mem_ctrl_t* controller = pg_master();
  pg_hdr_t* current_page = controller->page_list;
  while (current_page) {
    kma_page_t* page = *(kma_page_t**)current_page->this;
    current_page = current_page->next;
    free_page(page);
  }
  entry_page = NULL;
}

#endif // KMA_P2FL
