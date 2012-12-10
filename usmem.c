#include <stdlib.h>
#include <string.h>
#define INIT_OBJS       20
#define MAX_CHUNKS      255
#define CHUNK_TOP       2048

#define obj_allocate        us_alloc
#define obj_deallocate      us_dealloc
#define new_obj_allocator   new_us_allocator

static void *
Malloc(size_t size) {
    void *p;
    if((p = malloc(size)) == NULL) {
        puts("Out of Memory");
        exit(0);
    }
    return p;
}

static void *
Realloc(void *p, size_t size) {
    void *t;
    if((t = realloc(p, size)) == NULL) {
        puts("Out of Memory");
        exit(0);
    }
    return t;
}
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

static void
add_new_fixed(pFixedAllocator fixObj, pObjAllocator objAlloc) {
    if(objAlloc->size == 0) {
        objAlloc->size = INIT_OBJS; 
        objAlloc->pool = (pFixedAllocator) Malloc(sizeof(FixedAllocator) * objAlloc->size);
    } else if(objAlloc->size <= objAlloc->fixCount) {
        objAlloc->size += INIT_OBJS;
        objAlloc->pool = (pFixedAllocator) Realloc(objAlloc->pool, sizeof(FixedAllocator) * objAlloc->size);
    }
    memcpy(&objAlloc->pool[objAlloc->fixCount], fixObj, sizeof(FixedAllocator));
    if(objAlloc->fixCount > 0) {
        objAlloc->pool[objAlloc->fixCount].prev = &objAlloc->pool[objAlloc->fixCount - 1];
        objAlloc->pool[objAlloc->fixCount - 1].next = &objAlloc->pool[objAlloc->fixCount];
    }
    objAlloc->fixCount++;
}

static void
add_new_chunk(pFixedAllocator fixObj, pChunk chunk) {
    if(fixObj->size == 0) {
        fixObj->size = INIT_OBJS;
        fixObj->chunks = (pChunk) Malloc(sizeof(Chunk) * fixObj->size);
    } else if(fixObj->size == fixObj->chunkCount) {
        fixObj->size += INIT_OBJS;
        fixObj->chunks= (pChunk) Realloc(fixObj->chunks, sizeof(Chunk) * fixObj->size);
    }
    memcpy(&fixObj->chunks[fixObj->chunkCount++], chunk, sizeof(Chunk));
}

static pChunk
init_chunk(size_t size){
    pChunk chunk = (pChunk) Malloc(sizeof(Chunk));
    int blocks = MAX_CHUNKS;
    if(size * blocks > CHUNK_TOP) {
        blocks = CHUNK_TOP / size;
    }
    blocks = (blocks == 0 ? 1 : blocks);
    chunk->pData = (unsigned char *) Malloc(sizeof(unsigned char) * size * blocks);
    chunk->firstAvailableBlock = 0;
    chunk->blocksAvailable = blocks;
    int i;
    unsigned char* tmp = chunk->pData;
    for(i = 0; i < blocks; ++i) {
        *tmp = i + 1;
        tmp += sizeof(unsigned char) * size;
    }
    return chunk;
}

static void*
chunk_malloc(pChunk chunk, size_t size) {
    if(chunk->blocksAvailable == 0) return NULL;
    unsigned char *p = chunk->pData + chunk->firstAvailableBlock * size;
    chunk->firstAvailableBlock  = *p;
    --chunk->blocksAvailable;
    return p;
}

static void
chunk_free(pChunk chunk, void *p, size_t size) {//to free p, ensure p is in chunk
    *(unsigned char*)p = chunk->firstAvailableBlock;
    chunk->firstAvailableBlock = ((unsigned char *)p - chunk->pData) / size;
    ++chunk->blocksAvailable;
}

static void *
fix_allocate(pFixedAllocator fixObj) {
    int i;
    pChunk target = NULL;
    if(fixObj->allocChunk != NULL && fixObj->allocChunk->blocksAvailable != 0) {
        target = fixObj->allocChunk;
    } else {
        for(i = 0; i < fixObj->chunkCount; ++i) {
            pChunk cur_chunk = &fixObj->chunks[i];
            if(cur_chunk->blocksAvailable != 0) {
                target = cur_chunk;
                break;
            }
        }
        if(target == NULL) {
            pChunk chunk = init_chunk(fixObj->blockSize);
            add_new_chunk(fixObj, chunk);
            free(chunk);
            target = &fixObj->chunks[fixObj->chunkCount - 1];
        }
    }
    fixObj->allocChunk = target;
    fixObj->deallocChunk = target;
    return chunk_malloc(target, fixObj->blockSize);
}

static void
fix_deallocate(pFixedAllocator fixObj, void *p) {
    int i;
    size_t range;
    int blocks = MAX_CHUNKS;
    if(fixObj->blockSize * blocks > CHUNK_TOP) {
        blocks = CHUNK_TOP / fixObj->blockSize;
    }
    blocks = (blocks == 0 ? 1 : blocks);
    pChunk cur_chunk;

    if(fixObj->deallocChunk != NULL) {
        cur_chunk = fixObj->deallocChunk;
        if(cur_chunk->pData <= (unsigned char *)p && (unsigned char*)p < cur_chunk->pData + blocks * fixObj->blockSize) {
            chunk_free(cur_chunk, p, fixObj->blockSize);
            return;
        }
    }

    for( i = 0; i < fixObj->chunkCount; ++i) {
        cur_chunk = &fixObj->chunks[i];
        if(cur_chunk->pData <= (unsigned char *)p && (unsigned char*)p < cur_chunk->pData + blocks * fixObj->blockSize) {
            fixObj->deallocChunk = cur_chunk;
            chunk_free(cur_chunk, p, fixObj->blockSize);
        }
    }
}


static pFixedAllocator 
vicinityFound(pObjAllocator objAlloc, pFixedAllocator anchor, size_t size) {
    if(anchor == NULL) {
        return NULL;
    }
    if(anchor->blockSize == size) {
        return anchor;
    }
    pFixedAllocator up = anchor->prev;
    pFixedAllocator down = anchor->next;
    for(; ; ) {
        if(up != NULL) {
            if(up->blockSize == size) {
                return up;
            }
            up = up->prev;
        }
        if(down != NULL) {
            if(down->blockSize == size) {
                return down;
            }
            down = down->next;
        }
        if(up == NULL && down == NULL) {
            return NULL;
        }
    }
}

void *
obj_allocate(pObjAllocator objAlloc, size_t size) {
    int i = 0;
    pFixedAllocator found = vicinityFound(objAlloc, objAlloc->pLastAlloc, size);

    if(found == NULL) {
        pFixedAllocator fix = (pFixedAllocator) calloc(1, sizeof(FixedAllocator));
        fix->blockSize = size;
        add_new_fixed(fix, objAlloc);
        free(fix);
        found = &objAlloc->pool[objAlloc->fixCount - 1];
    }
    objAlloc->pLastAlloc = found;
    return fix_allocate(found);
}

void
obj_deallocate(pObjAllocator objAlloc, void *p, size_t size) {
    int i = 0;
    pFixedAllocator found = NULL;
    for(i = 0; i < objAlloc->fixCount; ++i) {
        pFixedAllocator fix = &objAlloc->pool[i];
        if(fix->blockSize == size) {
            found = fix;
            break;
        }
    }
    if(found) {
        objAlloc->pLastDealloc = found;
        fix_deallocate(found, p);
        return;
    }
}

pObjAllocator 
new_obj_allocator() {
   return calloc(1, sizeof(ObjAllocator)); 
}
