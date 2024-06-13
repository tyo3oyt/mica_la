#include "stride.h"
typedef struct PCLoadAddr_type{
    ADDRINT pc;
    ADDRINT loadAddr;
}PCLoadAddr;
typedef struct PCStoreAddr_type{
    ADDRINT pc;
    ADDRINT storeAddr;
}PCStoreAddr;
PCLoadAddr* loadTable;
PCStoreAddr* storeTable;

ADDRINT lastLoadAddr; //global
ADDRINT lastStoreAddr; //global
uint32_t loadIndex; 
uint32_t storeIndex;
uint32_t loadTableSize;
uint32_t storeTableSize;

uint64_t localReadDistrib[MAX_DISTR];
uint64_t globalReadDistrib[MAX_DISTR];
uint64_t localWriteDistrib[MAX_DISTR];
uint64_t globalWriteDistrib[MAX_DISTR];

void stride_init(){
    // global
    lastLoadAddr = 0;
	lastStoreAddr = 0;
    //local
    loadIndex = 0;
    storeIndex = 0;
    loadTableSize = 1024;
    storeTableSize = 1024;
    loadTable = (PCLoadAddr*)checked_malloc(loadTableSize*sizeof(PCLoadAddr));
    storeTable = (PCStoreAddr*)checked_malloc(storeTableSize*sizeof(PCStoreAddr));
    uint32_t i ;
    for(i = 0 ; i < loadTableSize ; i++){
        loadTable[i].loadAddr = 0;
        loadTable[i].pc = 0;
    }
    for(i = 0 ; i < storeTableSize ; i++){
        storeTable[i].pc = 0;
        storeTable[i].storeAddr = 0;
    }
}
// stride
uint32_t find_load_inst_addr(ADDRINT pc){
    uint32_t i;
    for(i = 0 ; i < loadIndex ; i++){
        if(loadTable[i].pc == pc){
            break;
        }
    }
    //no find
    if(i == loadIndex){
        // register
        if(loadIndex >= loadTableSize){
            loadTableSize *= 2;
            PCLoadAddr* ptr = (PCLoadAddr*)checked_realloc(loadTable,loadTableSize*(sizeof(PCLoadAddr)));
            loadTable = ptr;
        }
        loadTable[loadIndex].pc = pc;
        loadTable[loadIndex].loadAddr = 0;
        loadIndex++;
    }
    return i;
}
uint32_t find_store_inst_addr(ADDRINT pc){
    uint32_t i;
    for(i = 0 ; i < storeIndex ; i++){
        if(storeTable[i].pc == pc){
            break;
        }
    }
    //no find
    if(i == storeIndex){
        // register
        if(storeIndex >= storeTableSize){
            storeTableSize *= 2;
            PCStoreAddr* ptr = (PCStoreAddr*)checked_realloc(storeTable,storeTableSize*(sizeof(PCStoreAddr)));
            storeTable = ptr;
        }
        storeTable[storeIndex].pc = pc;
        storeTable[storeIndex].storeAddr = 0;
        storeIndex++;
    }
    return i;
}
void store_stride(ADDRINT vaddr,uint32_t index,uint32_t size){
    ADDRINT stride;
    if(vaddr >= storeTable[index].storeAddr){
            stride = vaddr - storeTable[index].storeAddr;
        }
        else {
            stride = storeTable[index].storeAddr - vaddr;
        }
        if(stride >= MAX_DISTR - 1){
            stride = MAX_DISTR - 1;
        }
        localWriteDistrib[stride]++;
        storeTable[index].storeAddr = vaddr + size - 1;

        if(vaddr >= lastStoreAddr){
            stride = vaddr - lastStoreAddr;
        }
        else{
            stride = lastStoreAddr - vaddr;
        }
        if(stride >= MAX_DISTR - 1){
            stride = MAX_DISTR - 1;
        }
        globalWriteDistrib[stride]++;
        lastStoreAddr = vaddr;
}
void load_stride(ADDRINT vaddr,uint32_t index,uint32_t size){
    ADDRINT stride;
    if(vaddr >= loadTable[index].loadAddr){
        stride = vaddr - loadTable[index].loadAddr;
    }
    else {
        stride = loadTable[index].loadAddr - vaddr;
    }
    if(stride >= MAX_DISTR - 1){
        stride = MAX_DISTR - 1;
    }
    localReadDistrib[stride]++;
    loadTable[index].loadAddr = vaddr + size - 1;

    if(vaddr >= lastLoadAddr){
        stride = vaddr - lastLoadAddr;
    }
    else{
        stride = lastLoadAddr - vaddr;
    }
    if(stride >= MAX_DISTR - 1){
        stride = MAX_DISTR -1;
    }
    globalReadDistrib[stride]++;
    lastLoadAddr = vaddr;
}
void stride_exit(FILE* file){
        //output memory access stride
    fprintf(file,"\n---------------------------------------------------\n");
    fprintf(file,"\nlocal load stride : \n");
    fprintf(file,"<=0                 <=8                 <=64                <=512               <=4096              total\n");
    
    uint64_t sum = 0;
    for(int i = 0 ; i < MAX_DISTR; i++){
        sum += localReadDistrib[i];
        if(i == 1 || i == 8 || i == 64 || i == 512 || i == 4096){
            fprintf(file,"%-20lu",sum);
        }
    }
    fprintf(file,"%-20lu    \n",sum);
    
    sum = 0;
    fprintf(file,"\nglobal load stride : \n");
     for(int i = 0 ; i < MAX_DISTR; i++){
        sum += globalReadDistrib[i];
        if(i == 1 || i == 8 || i == 64 || i == 512 || i == 4096){
            fprintf(file,"%-20lu",sum);
        }
    }
    fprintf(file,"%-20lu    \n",sum);
    sum = 0;
    fprintf(file,"\nlocal store stride : \n");
    for(int i = 0 ; i < MAX_DISTR; i++){
        sum += localWriteDistrib[i];
        if(i == 1 || i == 8 || i == 64 || i == 512 || i == 4096){
            fprintf(file,"%-20lu",sum);
        }
    }
    fprintf(file,"%-20lu    \n",sum);
    
    sum = 0;
    fprintf(file,"\nglobal store stride : \n");
    for(int i = 0 ; i < MAX_DISTR; i++){
        sum += globalWriteDistrib[i];
        if(i == 1 || i == 8 || i == 64 || i == 512 || i == 4096){
            fprintf(file,"%-20lu",sum);
        }
    }
    fprintf(file,"%-20lu    \n",sum);
}