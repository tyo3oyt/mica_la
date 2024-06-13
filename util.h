#ifndef __UTIL_H__
#define __UTIL_H__

#include<inttypes.h>
#include<stdlib.h>
#include<stdio.h>
#include<stdbool.h>
#define ERROR_MSG(x) fprintf(stderr,"ERROR : %s\n",x);
#define BLOCK_SIZE 6
#define PAGE_SIZE 12
#define WRAP(x) #x
#define REWRAP(x) WRAP(x)
#define MAX_MEM_ENTRIES 65536
#define MAX_MEM_BLOCK 65536
#define MAX_MEM_TABLE_ENTRIES 12289
#define MAX_DISTR 4098
#define MAX_COMM_DIST 130
#define MAX_REG_USE 130
#define MAX_NUM_OPER 5
#define MAX_NUM_REGS 64
#define NUM_INSTR_DESTINATIONS 1
#define NUM_INSTR_SOURCES 3
#define BUCKET_CNT 19
#define MAX_HIST_LENGTH 12
#define NUM_HIST_LENGTHS 3
const uint32_t history_lengths[NUM_HIST_LENGTHS] = {4,8,12};
typedef uint64_t ADDRINT;
/* 
    预定义宏
    __BASE_FILE__:当前编译单元的主源文件名
    __FILE__:    当前源文件的名称
    __LINE__:    当前源文件中的行号
*/
#define LOCATION __BASE_FILE__ ":" __FILE__ ":" REWRAP(__LINE__)
#define checked_malloc(size) ({ void *result = malloc (size); if (__builtin_expect (!result, false)) { ERROR_MSG ("Out of memory at " LOCATION "."); exit (1); }; result; })
#define checked_strdup(string) ({ char *result = strdup (string); if (__builtin_expect (!result, false)) { ERROR_MSG ("Out of memory at " LOCATION "."); exit (1); }; result; })
#define checked_realloc(ptr, size) ({ void *result = realloc (ptr, size); if (__builtin_expect (!result, false)) { ERROR_MSG ("Out of memory at " LOCATION "."); exit (1); }; result; })


/* memory node struct */
typedef struct memNode_type{
	/* ilp */
	int32_t timeAvailable[MAX_MEM_ENTRIES];
	/* memfootprint */
	/* MAX_MEM_BLOCK = 2^16*/
	bool numReferenced [MAX_MEM_BLOCK];
} memNode;

typedef struct nlist_type {
	ADDRINT id;
	memNode* mem;
	struct nlist_type* next;
} nlist;

memNode* lookup(nlist** WorkingSetTable,ADDRINT key);
memNode* install(nlist** WorkingSetTable,ADDRINT key);


#endif