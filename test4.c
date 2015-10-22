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

 typedef struct
{
	int size;
	void* previous;
	void* next;
	void* pageFirst;

} blockList;

typedef struct
{
	void* thisOne;
	int pageIndex;
	int blockNumber;
	blockList* header;

} blockListHeader;


/************Global Variables*********************************************/

static kma_page_t*
entryAll = NULL;

/************Function Prototypes******************************************/

void*
kma_malloc(kma_size_t size);
void
addResource(void* ptr, int size);
void*
firstFit(int size);
void
deleteResource(void* ptr);
void
kma_free(void* ptr, kma_size_t size);


/************External Declaration*****************************************/

/**************Implementation***********************************************/

void*
kma_malloc(kma_size_t size)
{
  //We are using first fit
  int sizeNew = size + sizeof(void*);
  if(sizeNew > PAGESIZE)
  {
    return NULL;
    //printf("too large.\n");
  }
  if(entryAll==NULL)
  {
    //allocate a new page
    kma_page_t* pageNew = get_page();
    entryAll = pageNew;
	*((kma_page_t**)pageNew->ptr) = pageNew;
	blockListHeader* listBegin = (blockListHeader*)(pageNew->ptr);
	long int listBeginUpdated = (long int)listBegin + sizeof(blockListHeader);
	listBegin->header = (blockList*)listBeginUpdated;
	(*listBegin).pageIndex = 0;
	(*listBegin).blockNumber = 0;
    void* postoAdd = listBegin->header;
    int sizetoAdd = PAGESIZE - sizeof(blockListHeader);
    addResource(postoAdd, sizetoAdd);
	//printf("new page alloc.\n");
  }

  void* posFit = firstFit(size);
  blockList* blockCurrent = (blockList*)posFit;
  blockListHeader* pageCurrent = (*blockCurrent).pageFirst;
  (*pageCurrent).blockNumber += 1;
  return posFit;
}

void
addResource(void* ptr, int size)
{
  blockListHeader* pageTotal =(blockListHeader*)(entryAll->ptr);
  //Round up!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  long int dist = ((long int)ptr-(long int)pageTotal)/PAGESIZE;
  long int pageDist = (long int)pageTotal + dist*PAGESIZE;
  blockListHeader* pageTemp = (blockListHeader*)pageDist;
  void* posAdd = pageTotal->header;
  blockList* thisOne = ptr;
  (*thisOne).pageFirst = pageTemp;
  ((blockList*)ptr)->size = size;
  ((blockList*)ptr)->previous = NULL;
  if(ptr == posAdd)
  {
	((blockList*)ptr)->next = NULL;
	//printf("last pointer.\n");
	return;
  }
  else if(ptr < posAdd)
  {
	((blockList*)(pageTotal->header))->previous = (blockList*)ptr;
	((blockList*)ptr)->next = ((blockList*)(pageTotal->header));
	pageTotal->header = (blockList*)ptr;
	return;
  }
  else
  {
  	while(((blockList*)posAdd)->next != NULL)
  	{
		if(ptr < posAdd)
		{
			break;
		}
		posAdd = ((blockList*)posAdd)->next;
  	}
  	blockList* nextTemp = ((blockList*)posAdd)->next;
  	((blockList*)posAdd)->next = ptr;
  	((blockList*)ptr)->next = nextTemp;
  	((blockList*)ptr)->previous = posAdd;
  	if(nextTemp != NULL)
  	{
		nextTemp->previous = ptr;
  	}
  }

}

void*
firstFit(int size)
{
  blockListHeader* pageTotal = entryAll->ptr;
  blockList* posCheck = pageTotal->header;
  int sizeofBlock;
  //segment fault!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  if(size < sizeof(blockList))
  {
	size = sizeof(blockList);
  }
  while(posCheck != NULL)
  {
	sizeofBlock = posCheck->size;
    if(sizeofBlock >= size)
    {
      //fit found!
      //printf("find a fit for size:%d.\n", size);
      if((sizeofBlock == size) ||
      	((sizeofBlock-size) < sizeof(blockList)) )
      {
		    deleteResource(posCheck);
		    return ((void*)posCheck);
      }
      else
      {
      	long int posUpdated = (long int)posCheck + size;
      	void* posNew = (void*)posUpdated;
      	int sizeNew = sizeofBlock - size;
	  	addResource(posNew, sizeNew);
	  	deleteResource(posCheck);
      	return ((void*)posCheck);
      }
    }
    posCheck = posCheck->next;
  }
  //printf("no fit found.\ncreate new page.\n");
  kma_page_t* pageNew = get_page();
  *((kma_page_t**)pageNew->ptr) = pageNew;
  blockListHeader* listBegin = (blockListHeader*)(pageNew->ptr);
  long int listBeginUpdated = (long int)listBegin + sizeof(blockListHeader);
  listBegin->header = (blockList*)listBeginUpdated;
  (*listBegin).pageIndex = 0;
  (*listBegin).blockNumber = 0;
  void* postoAdd = listBegin->header;
  int sizetoAdd = PAGESIZE - sizeof(blockListHeader);
  addResource(postoAdd, sizetoAdd);
  pageTotal->pageIndex++;

  return firstFit(size);
}

void
deleteResource(void* ptr)
{
  blockList* temp = ptr;
  blockList* nextTemp = temp->next;
  blockList* previousTemp = temp->previous;
  if((previousTemp == NULL) && (nextTemp == NULL))
  {
  	//printf("delete only one.\n");
	blockListHeader* pageTotal = entryAll->ptr;
	pageTotal->header = NULL;
    entryAll = NULL;
    return;
  }
  else if(nextTemp == NULL)
  {
  	//printf("delete end.\n");
    previousTemp->next = NULL;
    return;
  }
  else if(previousTemp == NULL)
  {
  	//printf("delete head.\n");
    blockListHeader* pageTotal = entryAll->ptr;
    nextTemp->previous = NULL;
	pageTotal->header = nextTemp;
    return;
  }
  else
  {
  	nextTemp->previous = previousTemp;
  	previousTemp->next = nextTemp;
  	return;
  }
}


void
kma_free(void* ptr, kma_size_t size)
{
  //printf("cannot free yet.\n");
  addResource(ptr, size);
  blockListHeader* thisPage = ((blockList*)ptr)->pageFirst;
  thisPage->blockNumber -= 1;
  //release a page if it is free
  blockListHeader* pageAll = entryAll->ptr;
  blockListHeader* pageTemp;
  int pageInd = pageAll->pageIndex;
  bool nextPageNF = TRUE;
  while(nextPageNF == TRUE)
  {
	long int posUpdated = (long int)pageAll + pageInd*PAGESIZE;
	pageTemp = (blockListHeader*)posUpdated;
	blockList* posCheck = pageAll->header;
	blockList* posNext;
	//if page is free
	nextPageNF = FALSE;
	if(((blockListHeader*)pageTemp)->blockNumber == 0)
	{
		//printf("free page %d.\n", pageInd);
		while(posCheck != NULL)
		{
			posNext = posCheck->next;
			if(posCheck->pageFirst == pageTemp)
			{
				deleteResource(posCheck);
			}
			posCheck = posNext;
		}
		nextPageNF = TRUE;
		if(pageTemp == pageAll)
		{
			entryAll = NULL;
			nextPageNF = FALSE;
		}
		free_page((*pageTemp).thisOne);
		if(entryAll != NULL)
		{
			(pageAll->pageIndex)--;
		}
		pageInd -= 1;
		}
  }



}

#endif // KMA_RM