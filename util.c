#include "util.h"
memNode* lookup(nlist** WorkingSetTable,ADDRINT key){
    nlist* np;
	for (np = WorkingSetTable[key % MAX_MEM_TABLE_ENTRIES]; np != (nlist*)NULL; np = np->next){
		if(np-> id == key)
			return np->mem;
	}
	return (memNode*)NULL;
}
memNode* install(nlist** WorkingSetTable,ADDRINT key){

	nlist* np;
	ADDRINT index = key % MAX_MEM_TABLE_ENTRIES;
	np = WorkingSetTable[index];
    // np 指向一个即将install的内存空间
	if(np == (nlist*)NULL) {
		np = (nlist*)checked_malloc(sizeof(nlist));
		WorkingSetTable[index] = np;
	}
	else{
		while(np->next != (nlist*)NULL){
			np = np->next;
		}
		np->next = (nlist*)checked_malloc(sizeof(nlist));
		np = np->next;
	}
	np->next = (nlist*)NULL;
	np->id = key;
	np->mem = (memNode*)checked_malloc(sizeof(memNode));
	for(ADDRINT i = 0; i < MAX_MEM_ENTRIES; i++){
		(np->mem)->timeAvailable[i] = 0;
	}
	for(ADDRINT i = 0; i < MAX_MEM_BLOCK; i++){
		(np->mem)->numReferenced[i] = false;
	}
	return (np->mem);
}
