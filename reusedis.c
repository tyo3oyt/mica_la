#include "reusedis.h"
#include<stdio.h>
// data structure of reuse distance
typedef struct stack_entry_type{
    struct stack_entry_type* below;
    struct stack_entry_type* above;
    ADDRINT block_addr;
    uint32_t bucket;
}stack_entry;

typedef struct block_type_fast {
	ADDRINT id;
	stack_entry* stack_entries[MAX_MEM_ENTRIES];
	struct block_type_fast* next;
} block_fast;
static stack_entry* stack_top;
static uint64_t stack_size;
//hash table to store memory block have readed
static block_fast* hashTableCacheBlocks_fast[MAX_MEM_TABLE_ENTRIES];
static uint64_t mem_ref_cnt;
static uint64_t cold_refs;
static uint64_t buckets[BUCKET_CNT];
static stack_entry* borderline_stack_entries[BUCKET_CNT];


void reusedis_init(){
    cold_refs = 0;
    mem_ref_cnt = 0;
    int32_t i;
	for(i=0; i < BUCKET_CNT; i++){
		buckets[i] = 0;
		borderline_stack_entries[i] = NULL;
	}
	/* hash table */
	for (i = 0; i < MAX_MEM_TABLE_ENTRIES; i++) {
		hashTableCacheBlocks_fast[i] = NULL;
	}
	stack_top = (stack_entry*) checked_malloc(sizeof(stack_entry));
	stack_top->block_addr = 0;
	stack_top->above = NULL;
	stack_top->below = NULL;
	stack_top->bucket = 0;
	stack_size = 1;
}
stack_entry** entry_lookup(block_fast** table, ADDRINT key){

	block_fast* b;

	for (b = table[key % MAX_MEM_TABLE_ENTRIES]; b != NULL; b = b->next){
		if(b->id == key)
			return b->stack_entries;
	}
	return NULL;
}

/** entry_install**/
static stack_entry** entry_install(block_fast** table, ADDRINT key){

	block_fast* b;

	ADDRINT index = key % MAX_MEM_TABLE_ENTRIES;

	b = table[index];

	if(b == NULL) {
		b = (block_fast*)checked_malloc(sizeof(block_fast));
		table[index] = b;
	}
	else{
		while(b->next != NULL){
			b = b->next;
		}
		b->next = (block_fast*)checked_malloc(sizeof(block_fast));
		b = b->next;
	}
	b->next = NULL;
	b->id = key;
	for(ADDRINT i = 0; i < MAX_MEM_ENTRIES; i++){
		b->stack_entries[i] = NULL;
	}
	return b->stack_entries;
}
static void move_to_top_fast(stack_entry *e, ADDRINT a){

	uint32_t bucket;

	/* check if entry was accessed before */
	if(e != NULL){

		/* check to see if we already are at top of stack */
		if(e->above != NULL){ // not at stack top

			// disconnect the entry from its current position on the stack
			if (e->below != NULL) e->below->above = e->above;
			e->above->below = e->below;
            
			// adjust all borderline entries
			for(bucket=0; bucket < BUCKET_CNT && bucket < e->bucket; bucket++){
				borderline_stack_entries[bucket]->bucket++;
				borderline_stack_entries[bucket] = borderline_stack_entries[bucket]->above;
			}
			// if the entry touched was a borderline entry, new borderline entry is the one above the touched one
			if(e == borderline_stack_entries[e->bucket]){
				borderline_stack_entries[e->bucket] = borderline_stack_entries[e->bucket]->above;
			}
			// place new entry on top of LRU stack
			e->below = stack_top;
			e->above = NULL;
			stack_top->above = e;
			stack_top = e;
			e->bucket = 0;
		}
	}
	else{ // cold refenrence , allocate memory in lru stack
		stack_entry* e = (stack_entry*) checked_malloc(sizeof(stack_entry));
        // e is new stack top
		e->block_addr = a;
		e->above = NULL;
		e->below = stack_top;
		e->bucket = 0;
		stack_top->above = e;
		stack_top = e;
		stack_size++;

		// adjust all borderline entries that exist up until the overflow bucket
		// (which really has no borderline entry since there is no next bucket)
		// we retain the number of the first free bucket for next code
		for(bucket=0; bucket < BUCKET_CNT - 1; bucket++){
			if (borderline_stack_entries[bucket] == NULL) break;
			borderline_stack_entries[bucket]->bucket++;
			borderline_stack_entries[bucket] = borderline_stack_entries[bucket]->above;
		}

		// if the stack size has reached a boundary of a bucket, set the boundary entry for this bucket
		// the variable types are chosen deliberately large for overflow safety
		// at least they should not overflow sooner than stack_size anyway
		// overflow bucket boundar is never set
		if (bucket < BUCKET_CNT - 1)
		{
			uint64_t borderline_distance = ((uint64_t) 2) << bucket;
			if(stack_size == borderline_distance){
				// find the bottom of the stack by traversing from somewhere close to it
				stack_entry *stack_bottom;
				if (bucket) stack_bottom = borderline_stack_entries [bucket-1];
				       else stack_bottom = stack_top;
				while (stack_bottom->below) stack_bottom = stack_bottom->below;
				// the new borderline is the bottom of the stack
				borderline_stack_entries [bucket] = stack_bottom;
			}
		}
	}
}

void get_reuse_dis(ADDRINT effMemAddr, ADDRINT size){
	ADDRINT a, endAddr, addr, upperAddr, indexInChunk;
	stack_entry** chunk;
	stack_entry* entry_for_addr;

	/* Calculate index in cache addresses. The calculation does not
	 * handle address overflows but those are unlikely to happen. */
	addr = effMemAddr >> BLOCK_SIZE;
	endAddr = (effMemAddr + size - 1) >> BLOCK_SIZE;

	/* The hit is counted for all cache lines involved. */
	for(a = addr; a <= endAddr; a++){

		/* split the cache line address into hash key of chunk and index in chunk */
		upperAddr = a >> 16;
		indexInChunk = a ^(upperAddr << 16) ;

		chunk = entry_lookup(hashTableCacheBlocks_fast, upperAddr);
		if(chunk == NULL) chunk = entry_install(hashTableCacheBlocks_fast, upperAddr);
        //entry_for_addr有可能是NULL，是NULL表明是cold ref
		entry_for_addr = chunk[indexInChunk];

		/* determine reuse distance for this access (if it has been accessed before) */
		//int b = det_reuse_dist_bucket(entry_for_addr);
        if(entry_for_addr != NULL){
            buckets[entry_for_addr->bucket]++;
        }
        else{
            cold_refs++;
        }

		/* adjust LRU stack */
		/* as a side effect, can allocate new entry, which could have been NULL so far */
		move_to_top_fast(entry_for_addr, a);

		/* update hash table for new cache blocks */
		if(chunk[indexInChunk] == NULL) chunk[indexInChunk] = stack_top;
		mem_ref_cnt++;
	}
}
void reusedis_exit(FILE* file){
    // output cache line reuse distance
    int i;
    fprintf(file,"\n---------------------------------------------------\n");
    fprintf(file , "\nmemory reference count : %lu , cold references : %lu \n",mem_ref_cnt,cold_refs);
    fprintf(file , "\nRead memory LRU distance (Bucket count):\n");

	for(i=0; i < BUCKET_CNT; i++){
		fprintf(file , "%lu    ",buckets[i]);
	}

}