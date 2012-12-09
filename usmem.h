#ifndef __US_MEM_H__
#define __US_MEM_H__
#include <stddef.h>
typedef struct uws_chunk_allocator{
    unsigned char* pData;
    unsigned char firstAvailableBlock;
    unsigned char blocksAvailable;
} Chunk, *pChunk;

typedef struct uws_fixed_allocator{
    size_t blockSize;
    size_t size;
    size_t chunkCount;
    unsigned char numBlocks;
    pChunk chunks;
    pChunk allocChunk;
    pChunk deallocChunk;
    struct uws_fixed_allocator *prev;
    struct uws_fixed_allocator *next;
} FixedAllocator, *pFixedAllocator;

typedef struct uws_obj_allocator {
    pFixedAllocator pool;
    pFixedAllocator pLastAlloc;
    pFixedAllocator pLastDealloc;
    size_t size;
    size_t fixCount;
    size_t fixSize;
} ObjAllocator, *pObjAllocator;

typedef pObjAllocator us_allocator;

void * us_alloc(pObjAllocator objAlloc, size_t size);
void us_dealloc(pObjAllocator objAlloc, void *p, size_t size);
pObjAllocator new_obj_allocator();

#endif
