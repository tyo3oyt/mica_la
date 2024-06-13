#include "footprint.h"
// data structure of WSS
static nlist* DmemCacheWorkingSetTable[MAX_MEM_TABLE_ENTRIES];
static nlist* DmemPageWorkingSetTable[MAX_MEM_TABLE_ENTRIES];
static nlist* ImemCacheWorkingSetTable[MAX_MEM_TABLE_ENTRIES];
static nlist* ImemPageWorkingSetTable[MAX_MEM_TABLE_ENTRIES];
void footprint_init(){
    int32_t i;
    for (i = 0; i < MAX_MEM_TABLE_ENTRIES; i++) {
		DmemCacheWorkingSetTable[i] = (nlist*) NULL;
		DmemPageWorkingSetTable[i] = (nlist*) NULL;
		ImemCacheWorkingSetTable[i] = (nlist*) NULL;
		ImemPageWorkingSetTable[i] = (nlist*) NULL;
	}
}
void ins_mem_op(ADDRINT insnVaddr,uint32_t size){
    memNode* chunk;
    ADDRINT a , key,blockIndex,pageIndex;
    ADDRINT startaddr = insnVaddr >> BLOCK_SIZE; 
    ADDRINT endaddr = (insnVaddr + size - 1) >> BLOCK_SIZE;
    for(a = startaddr; a <= endaddr; a++){
        key = a >> 16;
        blockIndex = a ^ (key << 16);
        chunk = lookup(ImemCacheWorkingSetTable,key);
        if(chunk == (memNode*)NULL){
            chunk = install(ImemCacheWorkingSetTable,key);
        }
        chunk->numReferenced[blockIndex] = true;
    }

    startaddr = insnVaddr >> PAGE_SIZE;
    endaddr = (insnVaddr + size - 1) >> PAGE_SIZE;
    for(a  = startaddr; a <= endaddr; a++){
        key = a >> 16;
        pageIndex = a ^ (key << 16);
        chunk = lookup(ImemPageWorkingSetTable,key);
        if(chunk == (memNode*)NULL){
            chunk = install(ImemPageWorkingSetTable,key);
        }
        chunk->numReferenced[pageIndex] = true;
    }
}

void data_mem_op(ADDRINT vaddr,uint32_t size){
        memNode* chunk;
        ADDRINT a , key,blockIndex,pageIndex;
        ADDRINT startaddr = vaddr >> BLOCK_SIZE; 
        ADDRINT endaddr = (vaddr + size - 1) >> BLOCK_SIZE;
        for(a = startaddr; a <= endaddr; a++){
            key = a >> 16;
            blockIndex = a ^ (key << 16);
            chunk = lookup(DmemCacheWorkingSetTable,key);
            if(chunk == (memNode*)NULL){
                chunk = install(DmemCacheWorkingSetTable,key);
            }
            chunk->numReferenced[blockIndex] = true;
        }

        startaddr = vaddr >> PAGE_SIZE;
        endaddr = (vaddr + size - 1) >> PAGE_SIZE;
        for(a  = startaddr; a <= endaddr; a++){
            key = a >> 16;
            pageIndex = a ^ (key << 16);
            chunk = lookup(DmemPageWorkingSetTable,key);
            if(chunk == (memNode*)NULL){
                chunk = install(DmemPageWorkingSetTable,key);
            }
            chunk->numReferenced[pageIndex] = true;
        }
}

static long long get_DmemBlockWSS(){
    long long DmemBlockWSS = 0L;
	for (int i = 0; i < MAX_MEM_TABLE_ENTRIES; i++) {
		for (nlist *np = DmemCacheWorkingSetTable [i]; np != (nlist*) NULL; np = np->next) {
			for (ADDRINT j = 0; j < MAX_MEM_BLOCK; j++) {
				if ((np->mem)->numReferenced [j]) {
					DmemBlockWSS++;
				}
			}
		}
	}
	return DmemBlockWSS;
}
static long long get_ImemBlockWSS(){
    long long ImemBlockWSS = 0L;
	for (int i = 0; i < MAX_MEM_TABLE_ENTRIES; i++) {
		for (nlist *np = ImemCacheWorkingSetTable [i]; np != (nlist*) NULL; np = np->next) {
			for (ADDRINT j = 0; j < MAX_MEM_BLOCK; j++) {
				if ((np->mem)->numReferenced [j]) {
					ImemBlockWSS++;
				}
			}
		}
	}
	return ImemBlockWSS;
}
static long long get_DmemPageWSS(){
    long long DmemPageWSS = 0L;
	for (int i = 0; i < MAX_MEM_TABLE_ENTRIES; i++) {
		for (nlist *np = DmemPageWorkingSetTable [i]; np != (nlist*) NULL; np = np->next) {
			for (ADDRINT j = 0; j < MAX_MEM_BLOCK; j++) {
				if ((np->mem)->numReferenced [j]) {
					DmemPageWSS++;
				}
			}
		}
	}
	return DmemPageWSS;

}
static long long get_ImemPageWSS(){
    long long ImemPageWSS = 0L;
	for (int i = 0; i < MAX_MEM_TABLE_ENTRIES; i++) {
		for (nlist *np = ImemPageWorkingSetTable [i]; np != (nlist*) NULL; np = np->next) {
			for (ADDRINT j = 0; j < MAX_MEM_BLOCK; j++) {
				if ((np->mem)->numReferenced [j]) {
					ImemPageWSS++;
				}
			}
		}
	}
	return ImemPageWSS;
}
void footprint_exit(FILE* file){
    long long DWSSBlock = get_DmemBlockWSS();
    long long DWSSPage  = get_DmemPageWSS();
    long long IWSSBlock = get_ImemBlockWSS();
    long long IWSSPage  = get_ImemPageWSS();
    fprintf(file,"---------------------------------------------------\n");
    fprintf(file,"\nData stream working set size        : %-20lld (64B block)\n",DWSSBlock);
    fprintf(file,"\nData stream working set size        : %-20lld (4KB page )\n",DWSSPage);
    fprintf(file,"\nInstruction stream working set size : %-20lld (64B block)\n",IWSSBlock);
    fprintf(file,"\nInstruction stream working set size : %-20lld (4KB page )\n",IWSSPage);
}