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

/************Global Variables*********************************************/

static kma_page_t *page_entry = NULL;

/************Function Prototypes******************************************/

void *allocate_mem(kma_size_t);
void newPage_of_node(); 

/************External Declaration*****************************************/

/**************Implementation***********************************************/
struct page_header {
  kma_page_t *page;
};

struct node {
  void *addr;
  int size;
  struct node *prev;
  struct node *next;
};

struct rm_controller {
  int   used;
  int   free;
  struct node free_mem_list;
  struct node available_node_list;
  struct node page_list;
};

void *current_page_begin_addr(void *addr) {
  return (void*)((unsigned long)addr & ~((unsigned long)(PAGESIZE-1)));
}

void *current_page_end_addr(void *addr) {
  return (void*)((char*)current_page_begin_addr(addr) + PAGESIZE);
}

void list_append(struct node *newNode, struct node *list) {
  newNode->prev = list->prev;
  newNode->next = list;
  list->prev = newNode;
  newNode->prev->next = newNode;
}

void list_insert(struct node *newNode, struct node *list, int pos) {
  if(pos < 1)
    fprintf(stderr, "insert position is less than 1");
  else {
    struct node *curr;
    
    curr = list;
    int i = 1;

    while(i != pos) {
      curr = curr->next;
      i++;
      if(curr == list){
        fprintf(stderr, "the position is larger than list size");
        break;
      }
    }
    newNode->prev = curr;
    newNode->next = curr->next;
    curr->next = newNode;
    newNode->next->prev = newNode;
  }
}

void list_remove(struct node *node) {
  node->prev->next = node->next;
  node->next->prev = node->prev;
}

void init_page_entry() {
  struct page_header *header;
  struct rm_controller *rm;
  struct node *curr, *end;
  page_entry = get_page();

  /* allocate header and controller to entry page. */
  header = (struct page_header*)page_entry->ptr;
  rm = (struct rm_controller*)((char*)page_entry->ptr + sizeof(struct page_header));

  /* initial elements in header and controller */
  header->page = page_entry;
  rm->used = 0;
  rm->free = 0;
  rm->free_mem_list.prev = &(rm->free_mem_list);
  rm->free_mem_list.next = &(rm->free_mem_list);
  rm->available_node_list.prev = &(rm->available_node_list);
  rm->available_node_list.next = &(rm->available_node_list);
  rm->page_list.prev = &(rm->page_list);
  rm->page_list.next = &(rm->page_list);
  
  curr = (struct node*)((char*)rm + sizeof(struct rm_controller));
  end = (struct node*)((char*)current_page_end_addr(curr));

  /* initial rest of  nodes to available_node_list. */
  while(curr + 1 < end) {
    list_append(curr, &(rm->available_node_list));
    curr++;
  }
}

void *rm_info() {
  void *ptr;
  ptr = (struct rm_controller*)((char*)page_entry->ptr + sizeof(struct page_header));

  return ptr;
}

struct node *find_available_node() {
  struct rm_controller *rm;
  struct node *curr;

  rm = rm_info();

  /* if there are no more available node */
  if(rm->available_node_list.next == &(rm->available_node_list)) {
    newPage_of_node();
  }
  curr = rm->available_node_list.next;

  return curr;
}

void newPage_of_node() {
  struct rm_controller *rm;
  kma_page_t *page;
  struct page_header *header;
  struct node *curr, *end;
  
  rm = rm_info();
  page = get_page();
  header = (struct page_header*)page->ptr;
  header->page = page;
  curr = (struct node*)((char*)header + sizeof(struct page_header));
  end = (struct node*)((char*)current_page_end_addr(curr));
  while(curr + 1<end) {
    list_append(curr, &(rm->available_node_list));
    curr++;
  }
  curr = find_available_node();
  curr->addr = (void*)page;
  list_remove(curr);
  list_append(curr, &(rm->page_list));
}



  



void *allocate_mem(kma_size_t size) {
  struct node *curr;
  kma_page_t *page;
  struct page_header *header;
  struct rm_controller *rm = rm_info();
  void *ptr;
  int available = 0;
  
  /* find a free place to allocate request */
  curr = rm->free_mem_list.next;
  while(curr != &(rm->free_mem_list)) {
    if(curr->size >= size) {
      available = 1;
      break;
    }
    curr = curr->next;
  }

  /* if no more free memory, 
   * then create a new page to store.
   */
  if(available == 0) {
    page = get_page();
    header = (struct page_header*)page->ptr;
    header->page = page;
    curr = find_available_node();
    curr->addr = (void*)((char*)page->ptr + sizeof(struct page_header));
    curr->size = page->size - sizeof(struct page_header);
  }
    
  /* allocate memeory and resize current node's address and size */
  ptr = curr->addr;
  curr->addr = (void*)((char*)curr->addr + size);
  curr->size = curr->size - size;
  if(curr->size == 0) {
    if(available == 1)
      list_remove(curr);
    list_insert(curr, &(rm->available_node_list), 1);
  }
  else {
    if(available == 0) {
      list_remove(curr);
      list_append(curr, &(rm->free_mem_list));
    }
  }
  rm->used++;
  return ptr;
}



void*
kma_malloc(kma_size_t size)
{
  if(size > PAGESIZE - sizeof(void*)) {
      return NULL;
  }

  if(page_entry == NULL)
    init_page_entry(); 
  

  return allocate_mem(size);
}

void
kma_free(void* ptr, kma_size_t size)
{
  struct rm_controller *rm;
  struct node *curr, *node;
  struct page_header *header;
  void* end_addr;
//  kma_page_t pageFree[1000];
  int combine = 0;
  
  rm = rm_info();

  end_addr = (void*)((char*)ptr + size);
  node = NULL;

  // TODO verify the ptr and size validation.
  
  /* find if this memory piece is next to a free memory piece */
  curr = rm->free_mem_list.next;
  while(curr != &(rm->free_mem_list)) {
    if(curr->addr == end_addr) {
      if(combine == 0){
        combine = 1;
        curr->size = curr->size + size;
        curr->addr = ptr;
        node = curr;
      }
      else if(combine == 1){
        curr->size = curr->size + node->size;
        curr->addr = node->addr;
        list_remove(node);
        list_insert(node, &(rm->available_node_list), 1);
        break;
      }
    }
    else if(((char*)curr->addr + curr->size) == ptr) {
      if(combine == 0) {
        combine = 1;
        curr->size = curr->size + size;
        node = curr;
      }
      else if(combine == 1) {
        curr->size = curr->size + node->size;
        list_remove(node);
        list_insert(node, &(rm->available_node_list), 1);
        break;
      }
    }
      
    curr = curr->next;
  }

  if(combine == 0) {
    node = find_available_node();
    node->addr = ptr;
    node->size = size;
    list_remove(node);
    list_insert(node, &(rm->free_mem_list), 1);
  }
  rm->free++;
  
  if(rm->used == rm->free) {
    /* remove all page which used for allocate request memory */
    curr = rm->free_mem_list.next;
    while(curr != &(rm->free_mem_list)) {
      header = (struct page_header*)current_page_begin_addr(curr->addr);
      free_page(header->page);
      curr = curr->next;
    }
    /* remove all page used for store node */
    curr = rm->page_list.next;
    while(curr != &(rm->page_list)) {
      free_page(curr->addr);
      curr = curr->next;
    }

    free_page(page_entry);
    page_entry = NULL;

  }
}


#endif // KMA_RM