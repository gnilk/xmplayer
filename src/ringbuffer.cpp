/*-------------------------------------------------------------------------
 File    : $Archive: $
 Author  : $Author: $
 Version : $Revision: $
 Orginal : 2006-07-26, 15:50
 Descr   : Implementation of a dynamic ringbuffer (expanded on write if needed)
 
 
 Modified: $Date: $ by $Author: $
 ---------------------------------------------------------------------------
 TODO: [ -:Not done, +:In progress, !:Completed]
 <pre>
   - Ability to disallow expansion
   ! Read/Write in buffers (use memcpy) instead of loops
 </pre>
 
 
 \History
 - 10.05.09, FKling, Buffered read/writes, Added info routine
 - 07.05.09, FKling, Implementation
 
 ---------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>			// memcpy

#include "ringbuffer.h"
#include "mutex.h"

using namespace Goat;

RingBuffer::RingBuffer()
{
	sync.Initialize();
	szBlock = DEFAULT_RINGBUF_BLOCKSZ;
	szCapacity = 0;
	nBytes = 0;
	iReadCursor = 0;
	iWriteCursor = 0;
	pData = NULL;
	
	Lock();
	for(int i=0;i<4;i++) Expand();
	Unlock();
}
RingBuffer::~RingBuffer()
{
	if (pData != NULL)
	{
		free (pData);
	}
}

void RingBuffer::GetInfo(int *pReadCursor, int *pWriteCursor, int *pBytes, int *pCapacity)
{
	if (pReadCursor!=NULL) *pReadCursor = iReadCursor;
	if (pWriteCursor!=NULL) *pWriteCursor = iWriteCursor;
	if (pBytes!=NULL) *pBytes = nBytes;
	if (pCapacity!=NULL) *pCapacity = szCapacity;
}

int RingBuffer::GetBytes()
{
	return nBytes;
}

//
// Write data with expansion
//
void RingBuffer::Write(void *pSrcData, int iOffset, int nCount)
{
	Lock();
	// Make sure we have enough space for this write
	while (nCount > (szCapacity - nBytes)) Expand();
	
	// Write data to ring buffer 
	unsigned char *pSrc, *pDst;
	pSrc = (unsigned char *)pSrcData;
	pDst = (unsigned char *) pData;
	
	if ((iWriteCursor + nCount) > szCapacity)
	{
		int nFirst = (szCapacity - iWriteCursor) + 1;
		memcpy(&pDst[iWriteCursor], pSrc, nFirst);
				
		int nNext = nCount - nFirst;
		memcpy(pDst, &pSrc[nFirst], nNext);
		
		iWriteCursor = nNext;
	} else
	{
		memcpy(&pDst[iWriteCursor], pSrc, nCount);
		iWriteCursor += nCount;
	}
	
	nBytes += nCount;		   
	Unlock();
}


// Read data without locking - for internal use!
int RingBuffer::ReadNoLock(void *pDstData, int nCount)
{
	if (nCount > nBytes) nCount = nBytes;
	
	unsigned char *pSrc, *pDst;
	pSrc =  (unsigned char *)pData;
	pDst =  (unsigned char *)pDstData;
	if ((iReadCursor + nCount) > szCapacity)
	{
		// Exceed block, copy in two stages
		// First part it end of ringbuffer
		// Second part is from the beginning of ring buffer
		int nFirst = (szCapacity - iReadCursor)+1;
		memcpy(pDst, &pSrc[iReadCursor], nFirst);
		
		// Next
		int nNext = nCount - nFirst;
		memcpy(&pDst[nFirst], pSrc, nNext);
		
		iReadCursor += nNext;
	} else
	{
		memcpy(pDst, &pSrc[iReadCursor], nCount);
		iReadCursor += nCount;
	}
	
	// Don't consume - used by peek as well
	return nCount;
}

//
// Read & Consume data from buffer
//
int RingBuffer::Read(void *pDstData, int nCount)
{
	Lock();
	int res = ReadNoLock(pDstData, nCount);
	nBytes -= res;		   	
	Unlock();
	return res;
}

//
// Read data but don't consume it
//
int RingBuffer::Peek(void *pDstData, int nCount)
{
	int res;
	Lock();
	int tmpReadCursor = iReadCursor; // save
	res = ReadNoLock(pDstData, nCount);
	iReadCursor = tmpReadCursor; // restore
	Unlock();
	return res;
}

//
// Consume data but don't read
//
int RingBuffer::Forward(int nCount)
{
	Lock();
	if (nCount > nBytes) nCount = nBytes;

	for (int i=0;i<nCount;i++)
	{
		iReadCursor++;
		if (iReadCursor > szCapacity) iReadCursor=0;
	}
	nBytes -= nCount;
	Unlock();
	return nCount;
	
}

// ----------- Private functions

//
// Expand buffer size
//
void RingBuffer::Expand()
{
	pData = realloc(pData, szCapacity + szBlock);
	if (pData == NULL)
	{
		printf("RingBuffer::Expand, realloc");
		exit(1);
	}
	szCapacity += szBlock;
}
