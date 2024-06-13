#include <inttypes.h>
#include <assert.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <zlib.h>
#include "util.h"
#include "branch.h"
#include "register.h"
#include "reusedis.h"
#include "stride.h"
#include "footprint.h"
#include "loongarch_decode_insns.c.inc"
extern "C" {
#include "qemu-plugin.h"
}
uint32_t encode_reg(LA_OP op){
    if(op.type == LA_OP_GPR){
        return op.val;
    }
    else if(op.type == LA_OP_FR || op.type == LA_OP_VR || op.type == LA_OP_XR){
        return op.val + 32;
    }
    return 0;
}
uint64_t cur_ins_count = 0;
//instruction mix
enum{
    INST_BEGIN,
    INST_LOAD,
    INST_STORE,
    INST_BRANCH,
    INST_SIMD,
    INST_FLOAT,
    INST_INT,
    INST_END,
};
const char* inst_classes[] = {
    [INST_BEGIN] = "begin",
    [INST_LOAD] = "load",
    [INST_STORE]="store",
    [INST_BRANCH] = "branch",
    [INST_SIMD] = "simd",
    [INST_FLOAT] = "float",
    [INST_INT] = "int",
    [INST_END] = "end"
};
static uint64_t cat_count[INST_END];

typedef struct insn_reg_info
{
    ADDRINT inst_addr;
    uint32_t insn_code;
    unsigned char regreadcnt;
    unsigned char regwritecnt;
    unsigned char destination_registers[NUM_INSTR_DESTINATIONS];
    unsigned char source_registers[NUM_INSTR_SOURCES];
    struct insn_reg_info* next;
} reginfo_buffer_entry;

reginfo_buffer_entry* reg_buffer[MAX_MEM_TABLE_ENTRIES];
extern uint64_t* regOpNums;

//data structure of branch predictability
bool last_inst_is_branch_cond;
ADDRINT branch_inst_pc;
uint64_t branch_cond_cnt;

reginfo_buffer_entry* find_install_ins_buffer(ADDRINT pc,uint32_t opcode,LA_DECODE* la_decode){
    //search
    bool no_found = false;
    uint32_t key = pc % MAX_MEM_TABLE_ENTRIES;
    reginfo_buffer_entry* e = reg_buffer[key];
    reginfo_buffer_entry* tmp = e;
    if(e != NULL){
        while(e != (reginfo_buffer_entry*)NULL){
            if(e->inst_addr == pc){
                break; // find it !
            }
            e = e->next;
        }
        if(e == (reginfo_buffer_entry*)NULL){ // no found
            no_found = true;
			while(tmp->next != (reginfo_buffer_entry*)NULL) // find the last item
				tmp = tmp->next;
            e = (reginfo_buffer_entry*)checked_malloc(sizeof(reginfo_buffer_entry));
            e->inst_addr = pc;
            e->insn_code = 0;
            e->regreadcnt  = 0;
            e->regwritecnt = 0;
            for(int i = 0 ; i < NUM_INSTR_SOURCES ; i++){
                e->source_registers[i] = 0;
            }
            for(int i = 0 ; i< NUM_INSTR_DESTINATIONS ; i++){
                e->destination_registers[0] = 0;
            }
            e->next = NULL;
            tmp->next = e;
        }
    }
    else{ // no found and the key-list is empty
        no_found = true;
        e = (reginfo_buffer_entry*)checked_malloc(sizeof(reginfo_buffer_entry));
        e->inst_addr = pc;
        e->insn_code = 0;
        e->regreadcnt  = 0;
        e->regwritecnt = 0;
        for(int i = 0 ; i < NUM_INSTR_SOURCES ; i++){
            e->source_registers[i] = 0;
        }
        for(int i = 0 ; i< NUM_INSTR_DESTINATIONS ; i++){
            e->destination_registers[0] = 0;
        }
        e->next = NULL;
        reg_buffer[key] = e;
    }
    // no found and install
    if(no_found){
        uint32_t cnt = 0;
        e->inst_addr = pc;
        e->insn_code = opcode;
        // only have source registers , no destination registers
        if (la_inst_is_branch_not_link(la_decode->id) || la_inst_is_st(la_decode->id)) { 
            for (int i = 0; i < la_decode->opcnt && i < NUM_INSTR_SOURCES; i++) {
                e->source_registers[i] = encode_reg(la_decode->op[i]);
                cnt++;
            }
            e->regreadcnt = cnt;
            e->regwritecnt = 0;
        }
        else{
            cnt = 0;
            if (la_decode->opcnt >= 1){
                e->destination_registers[0] = encode_reg(la_decode->op[0]);
                e->regwritecnt = 1;
            }                                                                                    
            for (int i = 0; i < (la_decode->opcnt - 1) && i < NUM_INSTR_SOURCES; i++) {
                e->source_registers[i] = encode_reg(la_decode->op[i + 1]);
                cnt++;
            }
            e->regreadcnt = cnt;
        }
    }
    return e;
}


static void plugin_init(const qemu_info_t *info){
    uint32_t i;
    for (i = 1 ; i < INST_END ; i++){
        cat_count[i] = 0;
    }
    for(i = 0 ; i < MAX_MEM_TABLE_ENTRIES ; i++){
        reg_buffer[i] = (reginfo_buffer_entry*)NULL;
    }
    last_inst_is_branch_cond = false;
    branch_inst_pc = 0;
    branch_cond_cnt = 0;
    register_init();
    footprint_init();
    stride_init();
    reusedis_init();
    branch_init();
}

static void vcpu_insn_exec(unsigned int vcpu_index, void* userdata)
{
    //userdata
    reginfo_buffer_entry* e = (reginfo_buffer_entry*)userdata;
    cur_ins_count++;

    //用userdata传递指令地址
    ADDRINT insnVaddr = e->inst_addr;
    // userdata 传递指令二进制代码
    uint32_t insn_code = e->insn_code;
    uint32_t size = 4;
    ins_mem_op(insnVaddr,size);

    int i;
    for(i = 0 ; i < e->regreadcnt; i++){
        readRegOp_reg((uint32_t)e->source_registers[i]);
    }
    if(e->regwritecnt > 0){
        writeRegOp_reg((uint32_t)e->destination_registers[0]);
    }
    regOpNums[e->regreadcnt+e->regwritecnt]++;
    bool taken;
    if(last_inst_is_branch_cond){
        if(branch_inst_pc + 4 != insnVaddr){
            //taken
            taken = true;
        }
        else{
            //not taken
            taken = false;
        }
        instrument_ppm_cond_br(branch_inst_pc,taken);
    }
    uint32_t branch_op = insn_code >> 26;
    if((branch_op >= 16 && branch_op <= 18) || (branch_op >= 22 && branch_op <= 27)){
        last_inst_is_branch_cond = true;
        branch_inst_pc = insnVaddr;
        branch_cond_cnt++;
    }
    else {
        last_inst_is_branch_cond = false;
    }
}

// vaddr is memory access address
static void vcpu_mem_access(unsigned int vcpu_index, qemu_plugin_meminfo_t info,uint64_t vaddr, void* userdata) 
{
    // FOOTPRINT
    ADDRINT pc = (ADDRINT)userdata;
    uint32_t size = 1 << qemu_plugin_mem_size_shift(info); // byte
    
    if(size > 0){
        data_mem_op(vaddr,size);
    }
    uint32_t index;
    if(qemu_plugin_mem_is_store(info)){ // store ins
        index = find_store_inst_addr(pc);
        store_stride(vaddr,index,size);
    }
    else{ // load ins
        get_reuse_dis(vaddr,size);
        index = find_load_inst_addr(pc);
        load_stride(vaddr,index,size);
    }
}

static void tb_record(qemu_plugin_id_t id, struct qemu_plugin_tb *tb)
{
    size_t insns = qemu_plugin_tb_n_insns(tb);
    for (size_t i = 0; i < insns; i ++) {
        struct qemu_plugin_insn *insn = qemu_plugin_tb_get_insn(tb, i);
        uint64_t insn_vaddr = qemu_plugin_insn_vaddr(insn);

        const uint32_t* data = (uint32_t*)qemu_plugin_insn_data(insn);
        LA_DECODE la_decode = {};
        decode(&la_decode, *data);
        reginfo_buffer_entry* e = find_install_ins_buffer(insn_vaddr,*data,&la_decode);
        // printf("pc : 0x%" PRIx64 "    code : %" PRIx64 "\n",insn_vaddr,*data);
        if(la_inst_is_ld(la_decode.id)){
            qemu_plugin_register_vcpu_insn_exec_inline(insn, QEMU_PLUGIN_INLINE_ADD_U64, (void*)&cat_count[INST_LOAD], 1);
        }
        else if(la_inst_is_st(la_decode.id)){
            qemu_plugin_register_vcpu_insn_exec_inline(insn, QEMU_PLUGIN_INLINE_ADD_U64, (void*)&cat_count[INST_STORE], 1);
        }
        else if(la_inst_is_branch(la_decode.id)){
            qemu_plugin_register_vcpu_insn_exec_inline(insn, QEMU_PLUGIN_INLINE_ADD_U64, (void*)&cat_count[INST_BRANCH], 1);
        }
        else if(la_inst_is_simd(la_decode.id)){
            qemu_plugin_register_vcpu_insn_exec_inline(insn, QEMU_PLUGIN_INLINE_ADD_U64, (void*)&cat_count[INST_SIMD], 1);
        }
        else if(la_inst_is_float(la_decode.id)){
            qemu_plugin_register_vcpu_insn_exec_inline(insn, QEMU_PLUGIN_INLINE_ADD_U64, (void*)&cat_count[INST_FLOAT], 1);
        }
        else qemu_plugin_register_vcpu_insn_exec_inline(insn, QEMU_PLUGIN_INLINE_ADD_U64, (void*)&cat_count[INST_INT], 1);
        qemu_plugin_register_vcpu_insn_exec_cb(insn, vcpu_insn_exec, QEMU_PLUGIN_CB_NO_REGS, (void*)e);
        qemu_plugin_register_vcpu_mem_cb(insn, vcpu_mem_access, QEMU_PLUGIN_CB_NO_REGS, QEMU_PLUGIN_MEM_RW, (void*)insn_vaddr);
    }
}
QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;
static void plugin_exit(qemu_plugin_id_t id, void *p){
    FILE* file;
    file = fopen("uarchidp-characteristic.txt","a");
    // output instruction ratio
    fprintf(file,"---------------------------------------------------\n");
    uint64_t total_insn = 0;
    uint32_t i;
    for(i = 1; i < INST_END ; i++){
        total_insn += cat_count[i];
    }
    double ratio;
    for(int i = 1; i < INST_END ; i++){
        ratio = ((double)cat_count[i] / (double)total_insn ) * 100;        
        fprintf(file,"%-6s count : %-20lu   ratio : %2.2lf%%\n",inst_classes[i],cat_count[i],ratio);
    }
    footprint_exit(file);
    register_exit(file);
    stride_exit(file);
    reusedis_exit(file);
    branch_exit(file);
    fclose(file);
}
QEMU_PLUGIN_EXPORT
int qemu_plugin_install(qemu_plugin_id_t id, const qemu_info_t *info,int argc, char **argv)
{
    plugin_init(info);
    qemu_plugin_register_vcpu_tb_trans_cb(id, tb_record);
    qemu_plugin_register_atexit_cb(id, plugin_exit, NULL);
    return 0;
}