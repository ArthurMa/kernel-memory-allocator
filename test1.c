/****************************************************************************
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
//#include <time.h>
//#include <sys/time.h>
/************Private include**********************************************/
#include "kma_page.h"
#include "kma.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */

/* the blockheader for the memory blocks given to the user */
typedef struct blockheader
{
  struct blockheader* next; //next memory block
  struct blockheader* prev; //previous memory block
  int size;  // Size of memory to be used (doesn't include header)
  int allocated;  //is this block free (0) or used (1)? 
} blkhr;

/* the pageheader for the separate pages to hold blocks */
typedef struct pageheader
{
  int size;  //size of page, includes the pageheader
  int num_allocated;   //the number of allocated blocks
  kma_page_t* next_page;  //pointer to next page
  blkhr* blk_list;  //pointer to its block list
} kma_pageheader;


/* Macros to help with address manipulation */
#define GET(addr) (* (addr))
#define PUT(addr,val) (*(addr) = (val)) 
#define ADDR(var) (&var)
#define BLOCK(blk) ((blkhr*)(blk))
/************Global Variables*********************************************/

/* Keep track of the front of the page list */
static kma_page_t* front = NULL;

/* Variable to help with timestamping processes
static double the_time = 0;
static int num_requests = 0;
static double max_time = 0;
static double the_time_free = 0;
static double max_time_free = 0;
typedef unsigned long long timestamp_t;
*/
/************Function Prototypes******************************************/

blkhr* kma_find_fit(kma_size_t size);
void kma_insert(blkhr* block, kma_size_t size);
void kma_page_init(kma_page_t* page);
void kma_page_insert(kma_page_t* page);
void print_blocks();
void print_stack();
//static timestamp_t get_timestamp();
/************External Declaration*****************************************/

/**************Implementation***********************************************/

void*
kma_malloc(kma_size_t size)
{
  /* check if size is greater than page size */ 
  if ((size + sizeof(kma_page_t*)) > PAGESIZE) return NULL;

  if (!front)
  {
    // If there isn't a front yet, make a new one
    front = get_page();
    kma_page_init(front);
  }

  /* find a place in our pages to fit this memory based on its size */
  blkhr* block = kma_find_fit(size);

  if (block != NULL)
  {
    // we found a place for our block within our current pages, now insert
    kma_insert(block, size);
    // return the memory location, not the block header
    return (blkhr*)((int)block + sizeof(blkhr));
  }


  // since we couldn't find a place, we need to ask for a brand new page
  kma_page_t* new_page = get_page();
  if (new_page != NULL) { //as long as the operation succeeds...
    kma_page_init(new_page);
    kma_page_insert((kma_page_t*)(new_page->ptr));

    // now we can insert the block into the new page
    block = kma_find_fit(size);
    kma_insert(block, size);
    return (blkhr*)((int)block + sizeof(blkhr));
  }

  //all else fails, return NULL
  return NULL;
}

/* initialize when a new page is allocated */
/** @page = the page which we want to initialize **/
void kma_page_init(kma_page_t* page) 
{
  /* have the ptr point to the start of the page */
  /* need to have this so we can access our page in memory,
     not the stack location of our kma_page_t* */
  *((kma_page_t**)page->ptr) = page;   

  /* page size including the page header */
  int page_size = page->size - sizeof(kma_page_t*); 

  /*** intialize a page header to be put into the page ***/
  // @page_size = size of the page including the header
  // @0 = set num_allocated to 0
  // @NULL = set next_page to NULL because this is the last page
  // @(size set) = location of blk_list within the page  
  kma_pageheader page_hdr = 
          {page_size,
           0, 
           NULL, 
           page->ptr+sizeof(kma_page_t*)+sizeof(kma_pageheader)};
  
  // set the block size for blk_list, includes the block header
  int blk_size = page->size - sizeof(kma_pageheader) - sizeof(kma_page_t*);

  /*** initialize a block list to be put into the page ***/
  // @NULL = set next to NULL because it's the first block
  // @NULL = set prev to NULL because it's the first block
  // @blk_size = set the size of the blk_list to the size set above
  // @0 = this block hasn't been allocated yet
  blkhr blk_hdr = {NULL, NULL, blk_size, 0};

  //PUT these two items (page header and block header) on the page
  PUT((kma_pageheader*)(page->ptr+sizeof(kma_page_t*)), page_hdr);    
  PUT((blkhr*)(page->ptr+sizeof(kma_page_t*)+sizeof(kma_pageheader)),blk_hdr);
}

/* Take the block about to be freed and coalesce with possible other
   free blocks around it */
/* @block = the block about to be freed */
void coalesce(blkhr* block) 
{
  int coalesce_case = 0; 
  
  /*examine what case it is */
  if ((block->prev != NULL) && (!block->prev->allocated)) {
    if ((block->next != NULL) && (!block->next->allocated))
      coalesce_case = 3;  //next and previous blocks are free
    else
      coalesce_case = 1;  //previous block is free
  } else {
    if ((block->next != NULL) && (!block->next->allocated))
      coalesce_case = 2;  //next block is free
  }
  
  switch (coalesce_case)
  {
      /*case 1: prev block free */
      case 1: 
      {
          /* move blkhr pointers */
          block->prev->next = block->next;
          if (block->next != NULL) block->next->prev = block->prev;
          /* reset size, add up the sizes (current + previous) */   
          block->prev->size = block->size + block->prev->size; 
          block->prev->allocated = 0; //redundant line: make sure it's free
          return; 
      }
      /*case 2: next block free */ 
      case 2:
      {
          /* move pointers */
          if (block->next->next != NULL) {
              block->next->next->prev = block;
          }  

          block->size += block->next->size;
          block->next = block->next->next;
          /*reset size */
          block->allocated = 0;
          return;
      }
      /*case 3: prev and next block free */
      case 3:
      {
         /* move pointers */
         block->prev->next = block->next->next;
         if (block->next->next != NULL) block->next->next->prev = block->prev;    
         /* reset size */
         block->prev->size =+ block->size;
         if (block->next != NULL) block->prev->size += block->next->size;
         block->prev->allocated = 0; //redundant, just to make sure
         return;
      }
      /* do nothing */
      default: return;
  }
}

/* Free the given block from memory and remove from page's block list */
/* @ptr = pointer to the block of memory to free
   @size = size of the memory to free */
void kma_free(void* ptr, kma_size_t size)
{
  /* find the block header to the memory location */
  blkhr* hdr = (blkhr*)((int)ptr - sizeof(blkhr));
  hdr->allocated = 0;  //set it to free
  // now coalesce the free section
  coalesce(hdr);  

  //find the first block in the block list of the page
  while (hdr->prev != NULL)
  {
    hdr = hdr->prev;
  }

  /* find the header of the page */
  kma_pageheader* header_start = (kma_pageheader*)((int)hdr - sizeof(kma_pageheader));
  // now we can decrease the num_allocated variable of our page header
  int blk_count = --(header_start->num_allocated);

  /* we need to free the page if no more blocks in the page */
  if (blk_count == 0)  {
    //find that current page we're at
    kma_page_t** current_page = (kma_page_t**)((int)header_start - sizeof(kma_page_t*));
    //redundant, get the header for the page
    kma_pageheader* page_header = (kma_pageheader*)((void*)current_page + sizeof(kma_page_t*));

    // have curr and prev to find the current page so we can manipulate
    //    the linked list structure 
    kma_pageheader* curr = (kma_pageheader*)((void*)(front->ptr) + sizeof(kma_page_t*));
    kma_pageheader* prev = NULL;

    // loop through the pages until we find the one we want to evict
    while ((void*)curr != (void*)page_header && curr->next_page != NULL) {
        prev = curr;
     	curr = (kma_pageheader*)((void*)curr->next_page + sizeof(kma_page_t*));
    }

    // if it's the front page, then we need to reset the front page pointer
    if ((void*)current_page == front->ptr) {
      kma_page_t* next = ((kma_pageheader*)((void*)current_page + sizeof(kma_page_t*)))->next_page;
      if (next != NULL) {
        front = GET((kma_page_t**)next);
        front->ptr = next;
      }
    }

    // this also handles front page
    if (prev != NULL) {
      prev->next_page = page_header->next_page;
    }
    else {
      // reset the pointers of the linked list
      curr->next_page = page_header->next_page;
      if (curr->num_allocated == 0 && curr->next_page == NULL) {
        front = NULL;  //if this is the final page then we need to NULL front
      }
    }
    //now just free the page
    free_page(GET(current_page));
  }
}

/* Use first fit to find a place to set our block of memory to */
/* @size = size of memory we need to find */
blkhr* kma_find_fit(kma_size_t size)
{
  //start by getting the front page
  kma_page_t* current_page = (kma_page_t*)(front->ptr);

  // Loop through each page, looking to see if there's somewhere the block
  // could go.
  while(current_page != NULL) 
  {
    kma_pageheader* page_header = (kma_pageheader*)((int)current_page + sizeof(kma_page_t*));

    //find page with enough space in its memory block
    blkhr* current_block = page_header->blk_list;
    while (current_block != NULL)
    {
      if ((current_block->allocated == 0) && ((current_block->size - sizeof(blkhr)) >= size))
      {
        // Increment current page's number of allocated blocks
        page_header->num_allocated++;
      
        // Mark the current block as allocated
        current_block->allocated = 1;

        // Return the current block
        //printf("Returned block size: %d\n", current_block->size);
        return current_block;
      } 
      current_block = current_block->next;

    }

    current_page = page_header->next_page;
  }
  // If we can't find a free block, then return NULL; make a new page
  return NULL;
}

/* insert the block into the page we found it could fit into */
/* @block = pointer to the block we want to insert
   @size = the size we need to insert */
void kma_insert(blkhr* block, kma_size_t size) {
  // Get the size difference between what is free and what we need to insert
  int size_diff = block->size - (int)size - sizeof(blkhr); 

  // If there's room for us to divide the block up, then do it.
  // It needs to be bigger than the size of a block header so we
  //    can at least insert the overhead into the split up block
  if (size_diff > sizeof(blkhr)) {
    
    // create a pointer to the block location where it'll split
    blkhr* split_block = (blkhr*)((int)block + size + sizeof(blkhr));
    
    // Update the new block's prev, next, size and allocation setting
    split_block->next = block->next;
    split_block->prev = block;
    split_block->size = size_diff;
    split_block->allocated = 0;

    // Also update our current block with it's new size
    block->size = block->size - size_diff;
    // Our block's next is the new split block
    block->next = split_block;
    // Needed this so that we could account for
    // the block being pushed ahead to have the correct 'prev'
    if (split_block->next != NULL) split_block->next->prev = split_block;
  }
}

/* insert a page into the linked list of pages */
/* @page = pointer to the page to be inserted */
void kma_page_insert(kma_page_t* page)
{
  // Start with the front page and its header
  kma_page_t* current_page = (kma_page_t*)(front->ptr);
  kma_pageheader* page_header = (kma_pageheader*)((int)current_page + sizeof(kma_page_t*));

  //loop through until we find the last one
  while (page_header->next_page != NULL)
  {
    current_page = page_header->next_page;
    page_header = (kma_pageheader*)((int)current_page + sizeof(kma_page_t*));
  }
 
  //then simply insert it right in
  page_header->next_page = page;
}

/* debugging method for printing out all the blocks in all the pages */
void print_blocks() {
  printf("\n---- Status update ----\n");
  
  // Start with the front page
  kma_page_t* current_page = NULL;

  if (front != NULL) { 
    current_page = (kma_page_t*)(front->ptr);
    printf("Got here\n");
    printf("Front page at: %p\n\n", current_page);
  }
  else {
    printf("There are currently no pages.\n");
  }

  // Traverse all pages
  while(current_page != NULL) 
  {
    kma_pageheader* page_header = (kma_pageheader*)((int)current_page + sizeof(kma_page_t*));

    // Print the address of the current page
    printf("Loaded page at: %p with %d blocks allocated. Next page: %p\n", current_page, page_header->num_allocated, page_header->next_page);
    // Load the first block in that page
    blkhr* current_block = page_header->blk_list;
    int num_of_blocks = 0;
    while (current_block != NULL)
    {
      num_of_blocks++;
      printf("Block %d: %p. Size: %d. Next: %p Prev: %p. %s\n",
        num_of_blocks,
        (void*)current_block,
        current_block->size,
        current_block->next,
        current_block->prev, 
        current_block->allocated ? "Allocated" : "Free");
      // Advance to the next block
      current_block = current_block->next;
    }
    if (page_header->next_page == NULL) break;
    current_page = page_header->next_page;
    printf("\n");
  }
}

/* debugging information to print out information about our stack */
void print_stack() {
  // Start with the front page
  kma_page_t* current_page = NULL;

  if (front != NULL) { 
    current_page = (kma_page_t*)(front->ptr);
    printf("Got here\n");
    printf("Front page at: %p\n\n", current_page);
  }

  // Traverse all pages
  while(current_page != NULL) 
  {
    kma_pageheader* page_header = (kma_pageheader*)((int)current_page + sizeof(kma_page_t*));
    kma_page_t** page = (kma_page_t**)((int)page_header - sizeof(kma_page_t*));
    printf("stack: %p\n", GET(page));
    // Print the address of the current page
    current_page = page_header->next_page;
  }

}
/* Method for getting time stamps of processes
static timestamp_t get_timestamp()
{
  struct timeval now;
  gettimeofday(&now, NULL);
  return now.tv_usec + (timestamp_t)now.tv_sec * 1000000;
}
*/
#endif // KMA_RM