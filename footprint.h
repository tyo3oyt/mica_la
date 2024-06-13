#include "util.h"
void data_mem_op(ADDRINT vaddr,uint32_t size);
void ins_mem_op(ADDRINT insnVaddr,uint32_t size);
void footprint_exit(FILE* file);
void footprint_init();