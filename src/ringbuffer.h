#ifndef __GOAT_RINGBUFFER_H__
#define __GOAT_RINGBUFFER_H__

#include "mutex.h"

namespace Goat
{
#define DEFAULT_RINGBUF_BLOCKSZ 4096	// nr of bytes in each expand
	class RingBuffer
	{
	private:
		void *pData;
		int iWriteCursor, iReadCursor, nBytes;
		int szCapacity, szBlock;
		Mutex sync;		// Multithreaded sync
	private:
		void Expand();
		void Lock() { sync.Enter(); }
		void Unlock() { sync.Leave(); }
		
		int ReadNoLock(void *pDstData, int nBytes);
	
	public:
		RingBuffer();
		virtual ~RingBuffer();
		
		void Write(void *pSrcData, int iOffset, int nCount);
		int Read(void *pDstData, int nBytes);
		
		int Peek(void *pDstData, int nBytes);
		int Forward(int nBytes);
		int GetBytes();
		
		
		void GetInfo(int *pReadCursor, int *pWriteCursor, int *pBytes, int *pCapacity);
	};
}


#endif