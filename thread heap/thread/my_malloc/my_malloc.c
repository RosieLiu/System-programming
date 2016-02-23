#include <stdio.h> //needed for size_t
#include <unistd.h> //needed for sbrk
#include <assert.h> //For asserts
#include <errno.h> // for sbrk error detection
#include <pthread.h> //for mutex

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
static pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t arr_mutex[ARRAY_SIZE];
static pthread_mutex_t sbrk_mutex = PTHREAD_MUTEX_INITIALIZER;
static int extend_times = 0;

metadata_t * to_meta(void *bp);
footer_t * to_footer(void *bp);
void * to_block(metadata_t* mt);
void * meta_addr(void *bp);
size_t meta_size(metadata_t *mt);
size_t block_size(void *bp);
metadata_t * left_block(metadata_t *bp);
metadata_t * right_block(metadata_t *bp);
bool is_free(metadata_t *mt);
void set_free(metadata_t *mt);
void set_alloc(metadata_t *mt);
void set_size(metadata_t *mt, size_t s);
void delete_node(metadata_t *node, bool need_lock);
void add_node(metadata_t *node, bool need_lock);
metadata_t * find_fit(size_t space);//true: first fit, false: best fit
int array_idx(size_t space);
metadata_t * extend_heap(size_t space);
void *dmalloc(size_t numbytes);
void dfree(void *bp);


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

inline size_t block_size(void *bp) {
	return meta_size(to_meta(bp));
}

inline metadata_t * left_block(metadata_t *mt) {
	// return (void *)(meta_addr(bp) - FOOTER_SIZE - footer_size((footer_t *)(meta_addr(bp) - FOOTER_SIZE)));
	return (metadata_t *)((void *)mt - FOOTER_SIZE - meta_size((metadata_t *)((void *)mt - FOOTER_SIZE)) - META_SIZE);
}

inline metadata_t * right_block(metadata_t *mt) {
	// return (void *)(bp + block_size(bp) + FOOTER_SIZE + META_SIZE);
	return (metadata_t *)((void *)mt + META_SIZE + meta_size(mt) + FOOTER_SIZE);
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

void delete_node(metadata_t *node, bool need_lock) {
	size_t space = meta_size(node);
	int idx = array_idx(space);
	if (need_lock) {
		pthread_mutex_lock(&arr_mutex[idx]);
	}
	assert(is_free(node));
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
	set_alloc(node);
	set_alloc((metadata_t *)to_footer(to_block(node)));
	assert(!is_free(node));
	if (need_lock) {
		pthread_mutex_unlock(&arr_mutex[idx]);
	}
}

void add_node(metadata_t *node, bool need_lock) {
	int idx = array_idx(meta_size(node));
	if (need_lock) {
		pthread_mutex_lock(&arr_mutex[idx]);
	}
	assert(!is_free(node));
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
	set_free(node);
	set_free((metadata_t *)to_footer(to_block(node)));
	assert((metadata_t *)to_footer(to_block(node)));
	assert(is_free(node));
	if (need_lock) {
		pthread_mutex_unlock(&arr_mutex[idx]);
	}
}

metadata_t * find_fit(size_t space) {
    int idx = array_idx(space);
    metadata_t *flpt = NULL;
	size_t redundant = ~(size_t)0x00;
	metadata_t *result = NULL;
	while (idx < ARRAY_SIZE) {
		pthread_mutex_lock(&arr_mutex[idx]);

		flpt = freelist_arr[idx];
		while (flpt) {
			assert(is_free(flpt));
			assert(array_idx(meta_size(flpt)) == idx);


			if (meta_size(flpt) == space) {
				result = flpt;
				break;
			}
			else if (meta_size(flpt) > space && redundant > meta_size(flpt) - space) {
				result = flpt;
				redundant = meta_size(flpt) - space;
			}
			flpt = flpt->next;
		}
		if (result != NULL) {
			break;
		}
		pthread_mutex_unlock(&arr_mutex[idx]);
		idx++;
	}
	if (result) {
		assert(array_idx(meta_size(result)) == idx);
		assert(is_free(result));
		delete_node(result, false);
		pthread_mutex_unlock(&(arr_mutex[idx]));
	}
	else {
		if (!(result = extend_heap(space))) {
			printf("extend heap failed! \n");
			return NULL;
		}
	}
	return result;
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


void *ts_malloc(size_t numbytes) {
	return dmalloc(numbytes);
}


metadata_t * extend_heap(size_t space) {
	int numchunk = (space + META_SIZE + FOOTER_SIZE) / CHUNK_SIZE;
	if ((space + META_SIZE + FOOTER_SIZE) % CHUNK_SIZE > 0) {
		numchunk += 1;
	}
	size_t space_a = numchunk * CHUNK_SIZE;
	pthread_mutex_lock(&sbrk_mutex);
		void *last_brk = sbrk(space_a);
		if (errno == ENOMEM) {
			printf("no room for extending heap\n");
			pthread_mutex_unlock(&sbrk_mutex);
			return NULL;
		}
		metadata_t *epilogue = to_meta(last_brk + space_a);
		epilogue->prev = NULL;
		epilogue->next = NULL;
		set_alloc(epilogue);
		// should coalesce but not
		metadata_t *start1 = to_meta(last_brk);
		start1->prev = NULL;
		start1->next = NULL;
		set_alloc(start1);
		set_size(start1, space);
		footer_t *end1 = to_footer(to_block(start1));
		set_alloc((metadata_t *)end1);
		set_size((metadata_t *)end1, space);
	
		size_t space2 = space_a - 2*META_SIZE - 2*FOOTER_SIZE - space;
		metadata_t *start2 = (void *)end1 + FOOTER_SIZE;
		start2->prev = NULL;
		start2->next = NULL;
		set_alloc(start2);
		set_size(start2, space2);
		footer_t *end2 = to_footer(to_block(start2));
		set_alloc((metadata_t *)end2);
		set_size((metadata_t *)end2, space2);
		//////////////////////////////////////////////////////////////
		add_node(start2, true);
		extend_times += 1;
	pthread_mutex_unlock(&sbrk_mutex);
	return start1;
}

void* dmalloc(size_t numbytes) {
	if(!is_init) { 			//Initialize through sbrk call first time
		if(!dmalloc_init()) {
			return NULL;
		}
	}

	assert(numbytes > 0);

	size_t space = ALIGN(numbytes);
	metadata_t* flpt = find_fit(space);
	size_t rest = meta_size(flpt) - space;
	if (rest < ALIGN(META_SIZE + FOOTER_SIZE + 1)) {
		assert(!is_free(flpt));
	}
	else {
		rest = rest - META_SIZE - FOOTER_SIZE;
		// flpt and new flpt share the metadata, flpt and newmt shre the same footer
		// pthread_mutex_lock(&arr_mutex[array_idx(space)]); 
			set_size(flpt, space);
			set_alloc(flpt);
			set_size((metadata_t *)(to_footer(to_block(flpt))), space);
			set_alloc((metadata_t *)(to_footer(to_block(flpt))));
			metadata_t* newmt = (metadata_t *)((void *)flpt + META_SIZE + space + FOOTER_SIZE);
		// pthread_mutex_unlock(&arr_mutex[array_idx(space)]);

		// pthread_mutex_lock(&arr_mutex[array_idx(rest)]); 
			set_size(newmt, rest);
			set_alloc(newmt);
			set_size((metadata_t *)to_footer(to_block(newmt)), rest);
			set_alloc((metadata_t *)to_footer(to_block(newmt)));
			assert(!is_free(newmt));
			add_node(newmt, true);
		// pthread_mutex_unlock(&arr_mutex[array_idx(rest)]);
	}
	return to_block(flpt);
}


void ts_free(void* ptr) {
	dfree(ptr);
}

void dfree(void* ptr) {
	bool right_f = false, left_f = false;
	metadata_t *mt = to_meta(ptr);
	assert(!is_free(mt));
	int left_idx = array_idx(meta_size((metadata_t *)(ptr - META_SIZE - FOOTER_SIZE)));
	// perhaps context switch now!
	pthread_mutex_lock(&arr_mutex[left_idx]); 
		int left_size = meta_size(left_block(mt));
		if (is_free((metadata_t *)(ptr - META_SIZE - FOOTER_SIZE)) && 
			(left_idx == array_idx(meta_size((metadata_t *)(ptr - META_SIZE - FOOTER_SIZE)))))
		{
			// assert(left_size == meta_size(left_block(mt)));
			assert(is_free(left_block(mt)));
			delete_node(left_block(mt), false);
		// assume block 1 2 (3) 4 5, block 3 is current
		// if left index of 2 is wrong, it must be coalesced by 1, so must read through foot
			left_f = true;
		}
	pthread_mutex_unlock(&arr_mutex[left_idx]);
		
	
	int right_idx = array_idx(meta_size(right_block(mt)));
	pthread_mutex_lock(&arr_mutex[right_idx]);
		int right_size = meta_size(right_block(mt));
		if (is_free(right_block(mt)) && 
			(right_idx == array_idx(meta_size(right_block(mt)))) ) 
		{
			assert(is_free(right_block(mt)));
			delete_node(right_block(mt), false);
			right_f = true;
		}
	pthread_mutex_unlock(&arr_mutex[right_idx]);
	if (right_f)
		assert(!is_free(right_block(mt)));
	if (left_f)
		assert(!is_free(left_block(mt)));
	if (!left_f && !right_f) {
		assert(!is_free(mt));
	    add_node(mt, true);
	}
	else if (left_f && !right_f) {
		// int left_size =/ meta_size(left_block(mt));
		int new_size = left_size + FOOTER_SIZE + META_SIZE + meta_size(mt);
		pthread_mutex_lock(&arr_mutex[array_idx(new_size)]);
			set_alloc(left_block(mt));
			set_size(left_block(mt), new_size);
			set_alloc((metadata_t *)to_footer(ptr));
			set_size((metadata_t *)to_footer(ptr), new_size);
			assert(!is_free(left_block(mt)));
			add_node(left_block(mt), false);
		pthread_mutex_unlock(&arr_mutex[array_idx(new_size)]);
	}
	else if (!left_f && right_f) {
		// int right_size = meta_size(right_block(mt));
		int new_size = right_size + meta_size(mt) + FOOTER_SIZE + META_SIZE;
		pthread_mutex_lock(&arr_mutex[array_idx(new_size)]);
			set_alloc(mt);
			set_size(mt, new_size);
			set_alloc((metadata_t *)to_footer(to_block(mt)));
			set_size((metadata_t *)to_footer(to_block(mt)), new_size);
			assert(!is_free(mt));
			add_node(mt, false);
		pthread_mutex_unlock(&arr_mutex[array_idx(new_size)]);
	}
	else if (left_f && right_f) {
		int right_size = meta_size(right_block(mt));
		int left_size = meta_size(left_block(mt));
		int new_size = meta_size(mt) + left_size + right_size + 2 * (FOOTER_SIZE + META_SIZE);
		pthread_mutex_lock(&arr_mutex[array_idx(new_size)]);
			set_size((metadata_t *)to_footer(to_block(right_block(mt))), new_size);
			set_alloc((metadata_t *)to_footer(to_block(right_block(mt))));
			set_size(left_block(mt), new_size);
			set_alloc(left_block(mt));	
			assert(!is_free(left_block(mt)));
			add_node(left_block(mt), false);
		pthread_mutex_unlock(&arr_mutex[array_idx(new_size)]);
	}
}



bool dmalloc_init() {
 	
	pthread_mutex_lock(&init_mutex);
	if (is_init) {
		pthread_mutex_unlock(&init_mutex);
		return true;
	}

	int i = 0;
	for (i = 0; i < ARRAY_SIZE; i++) {
		pthread_mutex_init(&arr_mutex[i], NULL);
	}

	size_t max_bytes = ALIGN(MAX_HEAP_SIZE);
	i = 0;

	for (i = 0; i < ARRAY_SIZE; i++) {
	    freelist_arr[i] = NULL;
	}

	metadata_t *fl = (metadata_t*) sbrk(max_bytes); // returns heap_region, which is initialized to freelist
	heap_begin = fl;
	/* Q: Why casting is used? i.e., why (void*)-1? */
	if (fl == (void *)-1){
		pthread_mutex_unlock(&init_mutex);
		return false;
	}
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
	set_alloc(fl);
	set_size((metadata_t *)to_footer(to_block(fl)), meta_size(fl));	
	set_alloc((metadata_t *)to_footer(to_block(fl)));
	fl->next = NULL;
	fl->prev = NULL;
	add_node(fl, true);
	//set the initial flag
	is_init = true;
	pthread_mutex_unlock(&init_mutex);
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

