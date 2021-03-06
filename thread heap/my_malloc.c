#include <stdio.h> //needed for size_t
#include <unistd.h> //needed for sbrk
#include <assert.h> //For asserts
#include <errno.h> // for sbrk error detection
#include "my_malloc.h"


#define META_SIZE       (ALIGN(sizeof(metadata_t)))
#define FOOTER_SIZE     (ALIGN(sizeof(footer_t)))
#define ARRAY_SIZE      (60)
#define ARRAY_INIT		NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL
#define CHUNK_SIZE   (1024*4)
#define FF			 (1)
#define BF			 (2)
#define WF			 (3)

typedef struct metadata {
	size_t size; 
	struct metadata* next;
	struct metadata* prev; 
} metadata_t;

typedef struct footer {
    size_t size;
} footer_t;

static metadata_t* freelist_arr[ARRAY_SIZE] = {ARRAY_INIT, ARRAY_INIT};
static bool is_init = false;
static void * heap_begin;

metadata_t * to_meta(void *bp);
footer_t * to_footer(void *bp);
void * to_block(metadata_t* mt);
void * meta_addr(void *bp);
size_t meta_size(metadata_t *mt);
size_t footer_size(footer_t *ft);
size_t block_size(void *bp);
void * left_block(void *bp);
void * right_block(void *bp);
bool is_free(metadata_t *mt);
void set_free(metadata_t *mt);
void set_alloc(metadata_t *mt);
void set_size(metadata_t *mt, size_t s);
void delete_node(metadata_t *node);
void add_node(metadata_t *node);
metadata_t * find_fit(size_t space, int flag);//true: first fit, false: best fit
int array_idx(size_t space);
metadata_t * extend_heap(size_t space);
void *dmalloc(size_t numbytes, int flag);
void dfree(void *bp, int flag);


inline metadata_t * to_meta(void *bp) {
	return (metadata_t *)(bp - META_SIZE);
}

inline footer_t * to_footer(void *bp) {
	return (footer_t *)(bp + block_size(bp));
}

inline void * to_block(metadata_t *mt) {
	return (void *)(mt) + META_SIZE;
}

inline void * meta_addr(void *bp) {
	return bp - META_SIZE;
}

inline size_t meta_size(metadata_t *mt) {
	return mt->size & (~0x07);
}

inline size_t footer_size(footer_t *ft) {
	return ft->size & (~0x07);
}

inline size_t block_size(void *bp) {
	return meta_size(to_meta(bp));
}

inline void * left_block(void *bp) {
	return (void *)(meta_addr(bp) - FOOTER_SIZE - footer_size((footer_t *)(meta_addr(bp) - FOOTER_SIZE)));
}

inline void * right_block(void *bp) {
	return (void *)(bp + block_size(bp) + FOOTER_SIZE + META_SIZE);
}

inline bool is_free(metadata_t *mt) {
	if (mt == NULL) {
		return false;
	}
	return !(0x01 & (mt -> size)); 
}

inline void set_free(metadata_t *mt) {
	mt -> size &= ~0x01; //1: alloc  0: free
}

inline void set_alloc(metadata_t *mt) {
	mt -> size |= 0x01;  //1: alloc  0: free
}


inline void set_size(metadata_t *mt, size_t s) {
	int flag = mt -> size & 0x01;
	mt -> size = s;
	if (flag) {
		set_alloc(mt);
	}
	else {
		set_free(mt);
	}
}

inline void set_footer_size(metadata_t *mt, size_t s) {
	footer_t* ft = (footer_t *) ((void *)(mt) + META_SIZE + meta_size(mt) + FOOTER_SIZE);
	ft->size = s;
}

void delete_node(metadata_t *node) {
	size_t space = meta_size(node);
	int idx = array_idx(space);
	if (!node->prev && !node->next) {
		freelist_arr[idx] = NULL;
	}
	else if (!node->prev) {
		freelist_arr[idx] = node->next;
		node->next = NULL;
		freelist_arr[idx]->prev = NULL;
	}
	else if (!node->next) {
		node->prev->next = NULL;
		node->prev = NULL;
	}
	else {
		node->prev->next = node->next;
		node->next->prev = node->prev;
		node->prev = NULL;
		node->next = NULL;
	}
}

void add_node(metadata_t *node) {
	int idx = array_idx(meta_size(node));
	if (!freelist_arr[idx]) {
		freelist_arr[idx] = node;
		node->prev = NULL;
		node->next = NULL;
	}
	else {
		node->prev = NULL;
		node->next = freelist_arr[idx];
		freelist_arr[idx]->prev = node;
		freelist_arr[idx] = node;
	}
}

metadata_t * find_fit(size_t space, int flag) {
    int idx = array_idx(space);
    metadata_t *flpt = NULL;
    if (flag == FF) {
		while (idx < ARRAY_SIZE) {
			flpt = freelist_arr[idx];
			while (flpt) {
				if (meta_size(flpt) < space) {
					flpt = flpt->next;
				}
				else {
					return flpt;
				}
			}
			idx++;
		}
		return NULL;
    }
    else if (flag == BF) {
    	size_t redundant = ~(size_t)0x00;
    	metadata_t *result = NULL;
    	while (idx < ARRAY_SIZE && result == NULL) {
    		flpt = freelist_arr[idx];
    		while (flpt) {
    			if (meta_size(flpt) == space) {
    				return flpt;
    			}
    			else if (meta_size(flpt) > space && redundant > meta_size(flpt) - space) {
    				result = flpt;
    				redundant = meta_size(flpt) - space;			
    			}
    			flpt = flpt->next;
    		}
    		idx++;
    	}
    	return result;
    }
    else if (flag == WF) {
		metadata_t *result = NULL;
		int i = ARRAY_SIZE - 1;
		for (; i >= idx && !result; i--) {
			flpt = freelist_arr[i];
			while (flpt && !result) {
				if (meta_size(flpt) > space) {
					result = flpt;
				}
				flpt = flpt->next;
			}
		}
		return result;
	}
    return NULL;
}

int array_idx(size_t space) {
	int s = space >> 3;
	int result = 0;
	int i = 0;
    switch(s) {
    	case 0:
    		result = 0;
    		break;
    	case 1: case 2: case 3:
    		result = (s - 1);
    		break;
    	default:
			result = 0;
			while (s) {
				result += 1;
				s >>= 1;
			}
    }
    return result;
}

void *ff_malloc(size_t numbytes) {
	return dmalloc(numbytes, FF);
}

void *bf_malloc(size_t numbytes) {
	return dmalloc(numbytes, BF);
}

void *wf_malloc(size_t numbytes) {
	return dmalloc(numbytes, WF);
}

metadata_t * extend_heap(size_t space) {
	int numchunk = (space + META_SIZE + FOOTER_SIZE) / CHUNK_SIZE;
	if ((space + META_SIZE + FOOTER_SIZE) % CHUNK_SIZE > 0) {
		numchunk += 1;
	}
	size_t space_a = numchunk * CHUNK_SIZE;
	void *last_brk = sbrk(space_a);
	if (errno == ENOMEM) {
		printf("no room for extending heap\n");
		return NULL;
	}
	metadata_t *epilogue = to_meta(last_brk + space_a);
	metadata_t *start = to_meta(last_brk);
	start->prev = NULL;
	start->next = NULL;
	set_free(start);
	set_size(start, space_a - META_SIZE - FOOTER_SIZE);
	footer_t *end = to_footer(to_block(start));
	set_free((metadata_t *)end);
	set_size((metadata_t *)end, space_a - META_SIZE - FOOTER_SIZE);
	epilogue->prev = NULL;
	epilogue->next = NULL;
	set_alloc(epilogue);
	add_node(start);
	return start;
}

void* dmalloc(size_t numbytes, int flag) {
	if(!is_init) { 			//Initialize through sbrk call first time
		if(!dmalloc_init()) {
			return NULL;
		}
	}

	assert(numbytes > 0);

	/* Your code goes here */
	size_t space = ALIGN(numbytes);
	// go over the linked list, find the first fit
	//metadata_t* flpt = freelist;
	metadata_t* flpt = find_fit(space, flag);
	if (!flpt) {
		//return NULL;
		if (!(flpt = extend_heap(space))) {
			printf("extend heap failed! \n");
			return NULL;
		}
	}
	size_t rest = meta_size(flpt) - space;
	delete_node(flpt);
	if (rest < ALIGN(META_SIZE + FOOTER_SIZE + 1)) {
		set_alloc(flpt);
		set_alloc((metadata_t *)(to_footer(to_block(flpt))));
	}
	else {
		rest = rest - META_SIZE - FOOTER_SIZE;
		set_size(flpt, space);
		set_alloc(flpt);
		set_size((metadata_t *)(to_footer(to_block(flpt))), space);
		set_alloc((metadata_t *)(to_footer(to_block(flpt))));
		metadata_t* newmt = (metadata_t *)((void *)flpt + META_SIZE + space + FOOTER_SIZE);
		set_size(newmt, rest);
		set_free(newmt);
		set_size((metadata_t *)to_footer(to_block(newmt)), rest);
		set_free((metadata_t *)to_footer(to_block(newmt)));
		add_node(newmt);
	}
	return to_block(flpt);
}



void ff_free(void* ptr) {
	dfree(ptr, FF);
}

void bf_free(void* ptr) {
	dfree(ptr, BF);
}

void wf_free(void* ptr) {
	dfree(ptr, WF);
}

void dfree(void* ptr, int flag) {

	/* Your free and coalescing code goes here */
	//bool left_f = is_free(left_block(ptr));
	bool right_f = is_free(to_meta(right_block(ptr)));
	bool left_f = is_free((metadata_t *)(ptr - META_SIZE - FOOTER_SIZE));
	
	if (!left_f && !right_f) {
	    set_free(to_meta(ptr));
	    set_free((metadata_t *)to_footer(ptr));
	    add_node(to_meta(ptr));
	}
	else if (left_f && !right_f) {
		//process the linked list
		//change the size of two blocks
		delete_node(to_meta(left_block(ptr)));
		int left_size = block_size(left_block(ptr));
		int new_size = left_size + FOOTER_SIZE + META_SIZE + block_size(ptr);
		set_size(to_meta(left_block(ptr)), new_size);
		set_size((metadata_t *)to_footer(ptr), new_size);
		set_free(to_meta(left_block(ptr)));
		set_free((metadata_t *)to_footer(ptr));
		add_node(to_meta(left_block(ptr)));
	}
	else if (!left_f && right_f) {
		//process the linked list
		delete_node(to_meta(right_block(ptr)));
		//change the size of two blocks
		int right_size = block_size(right_block(ptr));
		int new_size = right_size + block_size(ptr) + FOOTER_SIZE + META_SIZE;
		set_size((metadata_t *)to_footer(right_block(ptr)), new_size);
		set_free((metadata_t *)to_footer(right_block(ptr)));
		set_size(to_meta(ptr), new_size);
		set_free(to_meta(ptr));
		//process the linked list
		add_node(to_meta(ptr));
	}
	else if (left_f && right_f) {
		//process the linked list
		delete_node(to_meta(right_block(ptr)));
		delete_node(to_meta(left_block(ptr)));
		//change the size of two blocks
		int right_size = block_size(right_block(ptr));
		int left_size = block_size(left_block(ptr));
		int new_size = block_size(ptr) + left_size + right_size + 2 * (FOOTER_SIZE + META_SIZE);
		set_size((metadata_t *)to_footer(right_block(ptr)), new_size);
		set_free((metadata_t *)to_footer(right_block(ptr)));
		set_size(to_meta(left_block(ptr)), new_size);
		set_free(to_meta(left_block(ptr)));	
		add_node(to_meta(left_block(ptr)));
	}
}


bool dmalloc_init() {

	/* Two choices: 
 	* 1. Append prologue and epilogue blocks to the start and the end of the freelist
 	* 2. Initialize freelist pointers to NULL
 	*
 	* Note: We provide the code for 2. Using 1 will help you to tackle the
 	* corner cases succinctly.
 	*/
 	
 	is_init = true;

	size_t max_bytes = ALIGN(MAX_HEAP_SIZE);
	int i = 0;
	for (i = 0; i < ARRAY_SIZE; i++) {
	    freelist_arr[i] = NULL;
	}
	
	metadata_t *fl = (metadata_t*) sbrk(max_bytes); // returns heap_region, which is initialized to freelist
	heap_begin = fl;
	/* Q: Why casting is used? i.e., why (void*)-1? */
	if (fl == (void *)-1)
		return false;
	//prologue block
	footer_t* prologue = (footer_t *)fl;
	set_size((metadata_t *)prologue, 0);
	set_alloc((metadata_t *)prologue);
	//epilogue block
	metadata_t* epilogue = (metadata_t *)((void *)fl + max_bytes - META_SIZE);
	set_size(epilogue, 0);
	set_alloc(epilogue);
	//freelist
	fl = (metadata_t *)((void *)fl + FOOTER_SIZE);
	set_size(fl, max_bytes - 2 * META_SIZE - 2 * FOOTER_SIZE);
	set_free(fl);
	set_size((metadata_t *)to_footer(to_block(fl)), meta_size(fl));	
	set_free((metadata_t *)to_footer(to_block(fl)));
	fl->next = NULL;
	fl->prev = NULL;
	add_node(fl);
	return true;
}



unsigned long get_data_segment_size(){
	void * heap_end = sbrk(0);
	unsigned long result = heap_end - heap_begin;
	return result;
}

unsigned long get_data_segment_free_space_size(){
	metadata_t *head = NULL;
	int i = 0;
	unsigned long result = 0;
	for (i = 0; i < ARRAY_SIZE; i++) {
		head = freelist_arr[i];
		while (head != NULL) {
			result += meta_size(head);
			head = head->next;
		}
	}
	return result;
}

void print_freelist() {
	metadata_t *freelist_head = NULL;
	int i = 0;
	for (i = 0; i < ARRAY_SIZE; i++) {
		freelist_head = freelist_arr[i];
		while (freelist_head != NULL) {
			DEBUG("  Array[%d], Freelist Size:%zd, Head:%p, Prev:%p, Next:%p\t", 
				i, freelist_head->size,freelist_head,freelist_head->prev,freelist_head->next);
			freelist_head = freelist_head->next;
		}
	}
	DEBUG("\n");
}

