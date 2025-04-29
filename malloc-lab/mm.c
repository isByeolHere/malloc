/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"
#include "config.h"


/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "team8",
    /* First member's full name */
    "Byeol Kim",
    /* First member's email address */
    "quf417@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4 /* Word and header/footer size (bytes) */
#define DSIZE 8 /* Double word size (bytes) */
#define CHUNKSIZE (1<<12) /* Extend heap by this amount (bytes) */
#define MINSIZE 16  

#define MAX(x, y) ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))
/* Read and write a word at address p */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp,compute addressof its header and footer */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

#define PREV(bp) (*(void**)(bp))          // bp가 가리키는 블록의 prev 포인터
#define NEXT(bp) (*(void**)(bp + WSIZE))  // bp가 가리키는 블록의 next 포인터

static char *heap_listp;
static char *free_listp = NULL;  // free 리스트의 시작 포인터
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
static void insert_free_block(void *bp);
static void remove_free_block(void *bp);

int mm_init(void);
void *mm_malloc(size_t size);
void mm_free(void *bp);
void *mm_realloc(void *ptr, size_t size);

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void) {
/* Create the initial empty heap */
  if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1){
    return -1;
    }
  PUT(heap_listp, 0);                        // 정렬 패딩
  PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); // 프롤로그 Header
  PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); // 프롤로그 Footer
  PUT(heap_listp + (3 * WSIZE), PACK(0, 1));     // Epilogue Header
  // PUT(heap_listp + (3 * WSIZE), PACK(MINSIZE, 0)); // 첫 가용 블록의 헤더
  // PUT(heap_listp + (4 * WSIZE), NULL);               // 이전 가용 블록의 주소
  // PUT(heap_listp + (5 * WSIZE), NULL);               // 다음 가용 블록의 주소
  // PUT(heap_listp + (6 * WSIZE), PACK(MINSIZE, 0)); // 첫 가용 블록의 푸터
  // PUT(heap_listp + (7 * WSIZE), PACK(0, 1));         // 에필로그 Header: 프로그램이 할당

  heap_listp += (2*WSIZE);
  free_listp = NULL;
  // printf("[mm_init] heap_listp=%p, free_listp=%p\n",
  //   (void*)heap_listp, (void*)free_listp);

  /* Extend the empty heap with a free block of CHUNKSIZE bytes */
  if (extend_heap(CHUNKSIZE/WSIZE) == NULL){
    return -1;
    // printf("마이너스-1");
  }
  // printf("[mm_init] heap_listp=%p, free_listp=%p\n", heap_listp, free_listp);
  return 0;  
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
  size_t asize; /* Adjusted block size */
  size_t extendsize; /* Amount to extend heap if no fit */
  char *bp;
  /* Ignore spurious requests */
  if (size == 0){return NULL;}
  
  /* Adjust block size to include overhead and alignment reqs. */
  if (size <= DSIZE){
    asize = MINSIZE;
  } else {
      // 헤더/푸터(DSIZE) + 요청 size 를 더한 후 ALIGNMENT(8) 단위로 올림
      asize = ALIGN(size + DSIZE);
      // 계산된 크기가 최소 블록 크기보다 작으면 최소 블록 크기로 설정
      asize = MAX(asize, MINSIZE);
  }
  
  /* Search the free list for a fit */
  if ((bp = find_fit(asize)) != NULL) {
    place(bp, asize);
    return bp;
  }
  /* No fit found. Get more memory and place the block */
  extendsize = MAX(asize,CHUNKSIZE);
  if ((bp = extend_heap(extendsize/WSIZE)) == NULL){
    return NULL;
  }
  place(bp, asize);
  return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
  size_t size = GET_SIZE(HDRP(ptr));

  PUT(HDRP(ptr), PACK(size, 0));
  PUT(FTRP(ptr), PACK(size, 0));
  // insert_free_block(ptr);    // free_list에 추가
  coalesce(ptr);   
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    // 1. 엣지 케이스 처리 (ptr == NULL 또는 size == 0)
    if (ptr == NULL) {
      return mm_malloc(size); // ptr이 NULL이면 malloc과 동일
  }
  if (size == 0) {
      mm_free(ptr); // size가 0이면 free와 동일
      return NULL;
  }

  // 2. 새 블록 할당
  void *newptr = mm_malloc(size);
  if (newptr == NULL) {
      return NULL; // 할당 실패 시 NULL 반환
  }

  // 3. 복사할 크기 계산
  size_t old_block_size = GET_SIZE(HDRP(ptr));
  // 이전 블록의 페이로드 크기 = 전체 크기 - 헤더(WSIZE) - 푸터(WSIZE)
  size_t old_payload_size = old_block_size - DSIZE; // DSIZE = WSIZE + WSIZE

  // 복사할 크기는 (요청된 새 크기 'size')와 (이전 페이로드 크기 'old_payload_size') 중 작은 값
  size_t copySize = (size < old_payload_size) ? size : old_payload_size;

  // 4. 데이터 복사
  memcpy(newptr, ptr, copySize);

  // 5. 이전 블록 해제
  mm_free(ptr);

  // 6. 새 블록 포인터 반환
  return newptr;
    // void *oldptr = ptr;
    // void *newptr;
    // size_t copySize;

    // newptr = mm_malloc(size);
    // if (newptr == NULL)
    //     return NULL;
    // copySize = *(size_t *)((char *)oldptr - DSIZE);
    // if (size < copySize)
    //     copySize = size;
    // memcpy(newptr, oldptr, copySize);
    // mm_free(oldptr);
    // return newptr;
}

static void *extend_heap(size_t words)
{
  char *bp;
  size_t size;
  /* Allocate an even number of words to maintain alignment */
  size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
  if (size < MINSIZE) size = MINSIZE; 
  /* 최소 블록(헤더+푸터+prev/next 포인터 공간) 보장 */
  // if (size < 2*DSIZE + 2*WSIZE)
  //     size = 2*DSIZE + 2*WSIZE;  // 예: 16(payload+헤더/푸터) + 8(prev/next)

  if ((long)(bp = mem_sbrk(size)) == -1){
    return NULL;
  }
  /* Initialize free block header/footer and the epilogue header */
  PUT(HDRP(bp), PACK(size, 0)); /* Free block header */
  PUT(FTRP(bp), PACK(size, 0)); /* Free block footer */
  PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));  /* New epilogue header */

  PREV(bp) = NULL;
  NEXT(bp) = NULL;

  // printf("[extend_heap] new block at bp=%p, size=%zu, epilogue at %p\n",
  //   bp, size, (void*)HDRP(NEXT_BLKP(bp)));
  /* Coalesce if the previous block was free */
  return coalesce(bp);
}

 static void *coalesce(void *bp)
{
  size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
  size_t size = GET_SIZE(HDRP(bp));
  // printf("[coalesce enter] bp=%p, HDRP(bp)=%p, FTRP(bp)=%p\n",
  //         bp, (void*)HDRP(bp), (void*)FTRP(bp));
  if (prev_alloc && next_alloc) { /* Case 1 */
  insert_free_block(bp);
  return bp;
  }

  else if (prev_alloc && !next_alloc) { /* Case 2 */
    remove_free_block(NEXT_BLKP(bp)); // next 블록 free_list에서 제거
    size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
  }

  else if (!prev_alloc && next_alloc) { /* Case 3 */
    remove_free_block(PREV_BLKP(bp)); // prev 블록 free_list에서 제거
    size += GET_SIZE(HDRP(PREV_BLKP(bp)));
    PUT(FTRP(bp), PACK(size, 0)); // 현재 블록 푸터 재설정
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); // 이전 블록 헤더 재설정
    bp = PREV_BLKP(bp); // 이전 블록의 시작점으로 포인터 변경
  }

  else { /* Case 4 */
  remove_free_block(PREV_BLKP(bp));
  remove_free_block(NEXT_BLKP(bp));
  size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
  PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
  PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
  bp = PREV_BLKP(bp);
  }
  insert_free_block(bp); 
  return bp;
}
 
static void *find_fit(size_t asize)
{
  /* First-fit search */
  void *bp = free_listp;
  while (bp != NULL) {
    if (asize <= GET_SIZE(HDRP(bp))) {
        return bp;
    }
    bp = NEXT(bp);
  }
  return NULL; /* No fit */
}

// MARK : 묵시적 리스트
// static void place(void *bp, size_t asize)
// {
//     size_t csize = GET_SIZE(HDRP(bp));

//     remove_free_block(bp);
  
//     if ((csize - asize) >= (4*DSIZE)) {
//     PUT(HDRP(bp), PACK(asize, 1));
//     PUT(FTRP(bp), PACK(asize, 1));
//     bp = NEXT_BLKP(bp);
//     PUT(HDRP(bp), PACK(csize-asize, 0));
//     PUT(FTRP(bp), PACK(csize-asize, 0));
//     insert_free_block(bp);  
//     }
//     else {
//       PUT(HDRP(bp), PACK(csize, 1));
//       PUT(FTRP(bp), PACK(csize, 1));
//       }
// }


// MARK : 명시적 리스트
static void place(void *bp, size_t asize)
{
  remove_free_block(bp); // free_list에서 해당 블록 제거

    size_t csize = GET_SIZE(HDRP(bp)); // 현재 블록의 크기

    if ((csize - asize) >= (MINSIZE)) // 차이가 최소 블록 크기 16보다 같거나 크면 분할
    {
        PUT(HDRP(bp), PACK(asize, 1)); // 현재 블록에는 필요한 만큼만 할당
        PUT(FTRP(bp), PACK(asize, 1));

        void *next_bp = NEXT_BLKP(bp); // 분할된 뒷부분 블록 포인터
        PUT(HDRP(next_bp), PACK(csize - asize, 0)); // 남은 크기를 다음 블록에 할당(가용 블록)
        PUT(FTRP(next_bp), PACK(csize - asize, 0));
        // 분할된 새 가용 블록의 PREV/NEXT 초기화는 coalesce에서 해주므로 여기선 불필요할 수 있으나,
        // 안전하게 하려면 여기서도 초기화하거나, insert_free_block이 처리하도록 확인.
        // 현재 insert_free_block 이 PREV/NEXT 설정하므로 여기서는 헤더/푸터만 설정해도 됨.
        insert_free_block(next_bp); // 남은 블록을 free_list에 추가
    }
    else // 분할하지 않고 전체 블록 사용
    {
        PUT(HDRP(bp), PACK(csize, 1)); // 해당 블록 전부 사용
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

static void insert_free_block(void *bp)
{
    NEXT(bp) = free_listp;    // bp의 succ를 원래 free_listp로
    PREV(bp) = NULL;          // bp는 새로 넣는 거니까 pred 없음

    if (free_listp != NULL)   // 원래 free_listp가 있었다면
        PREV(free_listp) = bp;  // 원래 첫번째 블록의 pred를 bp로 갱신

    free_listp = bp;          // free_listp를 bp로 업데이트
    // printf("[insert_free_block] bp=%p, free_listp=%p\n", bp, free_listp);
}

static void remove_free_block(void *bp)
{
    if (PREV(bp))  // bp의 앞에 다른 블록이 있다면
        NEXT(PREV(bp)) = NEXT(bp);
    else           // bp가 free_listp라면
        free_listp = NEXT(bp);

    if (NEXT(bp))  // bp의 다음 블록이 있다면
        PREV(NEXT(bp)) = PREV(bp);
}
