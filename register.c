#include "register.h"
#include<stdio.h>
// data structure of register
uint64_t* regOpNums;
bool* regRef;
uint64_t* PCTable;// PC , production , record the first generation ins count
uint64_t* regUseCnt; // usege counters for each register
uint64_t* regUseDistr; // distribution of register usage
uint64_t* regAgeDistr;
extern uint64_t cur_ins_count;

void register_init(){
    regOpNums   = (uint64_t*)checked_malloc(MAX_NUM_OPER * sizeof(uint64_t)); //MAX_NUM_OPER = 5,LoongArch中最多有四个寄存器操作数。
    regRef      = (bool*)checked_malloc(MAX_NUM_REGS * sizeof(bool));    //是否被读
    PCTable     = (uint64_t*)checked_malloc(MAX_NUM_REGS * sizeof(uint64_t));  // 记录一个寄存器新值被product时的动态指令数
    regUseCnt   = (uint64_t*)checked_malloc(MAX_NUM_REGS * sizeof(uint64_t));  // 记录一个寄存器在被product后，被读（use）的次数
    regUseDistr = (uint64_t*)checked_malloc(MAX_REG_USE * sizeof(uint64_t));   // 记录寄存器被use次数的分布
    regAgeDistr = (uint64_t*)checked_malloc(MAX_COMM_DIST * sizeof(uint64_t)); //记录寄存器真依赖关系之间间隔多少条动态指令的分布
    int32_t i;
    for(i = 0; i < MAX_NUM_OPER; i++){
		regOpNums[i] = 0;
	}
	for(i = 0; i < MAX_NUM_REGS; i++){
		regRef[i] = false;
		PCTable[i] = 0;
		regUseCnt[i] = 0;
	}
	for(i = 0; i < MAX_REG_USE; i++){
		regUseDistr[i] = 0;
	}
	for(i = 0; i < MAX_COMM_DIST; i++){
		regAgeDistr[i] = 0;
	}
}

void readRegOp_reg(uint32_t regid){
    if(regid == 0){
        return;
    }
    if(cur_ins_count < PCTable[regid]){
        abort();
    }
    uint64_t age = cur_ins_count - PCTable[regid];
    if(age >= MAX_COMM_DIST - 1){
        age = MAX_COMM_DIST - 1;
    }
    regAgeDistr[age]++;
    regUseCnt[regid]++;
    regRef[regid] = 1;
}
void writeRegOp_reg(uint32_t regid){
    if(regid == 0){
        return;
    }
    uint64_t num;
    if(regRef[regid]){
        num = regUseCnt[regid];
        if(num >= MAX_REG_USE - 1){
            num = MAX_REG_USE -1; 
        }
        regUseDistr[num]++;
    }
    PCTable[regid] = cur_ins_count;
    regUseCnt[regid] = 0;
    regRef[regid] = 1;
}
void register_exit(FILE* file){
    fprintf(file,"---------------------------------------------------\n");
    // output register traffic
    uint64_t totNumOps = 0;
    uint64_t totUseTimes = 0;
    double aveUseTimes;
    double aveOperandCnt;
	uint64_t num;
    int i;
	/* total number of register operands */
	for(i = 1; i < MAX_NUM_OPER; i++){
		totNumOps += regOpNums[i]*i;
	}
    // average operands = totNumOps/totInst;
    aveOperandCnt = (double)totNumOps/(double)cur_ins_count;
    fprintf(file,"\naverage register(GPR and FPR) operations : %.2lf\n",aveOperandCnt);
	
    /* average degree of use 一个寄存器每次被写之后，平均会被读（use）多少次*/
	num = 0;
	for(i = 0; i < MAX_REG_USE; i++){
		num += regUseDistr[i];
        totUseTimes += i * regUseDistr[i];
	}
    aveUseTimes = (double)totUseTimes/(double)num;
    fprintf(file,"\naverage degree of use: %.2lf\n",aveUseTimes);
	// ** register dependency distributions **
	num = 0;
	for(i = 0; i < MAX_COMM_DIST; i++){
		num += regAgeDistr[i];
	}

    fprintf(file,"\nregister dependecy distribution : \n");
    fprintf(file,"\n<=1                 <=2                 <=4                 <=8                 <=16                <=32                <=64                total\n");
	num = 0;
	for(i = 0; i < MAX_COMM_DIST; i++){
		num += regAgeDistr[i];
		if( (i == 1) || (i == 2) || (i == 4) || (i == 8) || (i == 16) || (i == 32) || (i == 64)){
			fprintf(file,"%-20lu",num);
		}
	}
    fprintf(file,"%-20lu\n",num);
}