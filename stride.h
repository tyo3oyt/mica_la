#include "util.h"
void stride_exit(FILE* file);
void stride_init();
void store_stride(ADDRINT vaddr,uint32_t index,uint32_t size);
void load_stride(ADDRINT vaddr,uint32_t index,uint32_t size);
uint32_t find_store_inst_addr(ADDRINT pc);
uint32_t find_load_inst_addr(ADDRINT pc);