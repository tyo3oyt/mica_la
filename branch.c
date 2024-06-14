#include "branch.h"



bool lastInstBr; // was the last instruction a cond. branch instruction?
ADDRINT nextAddr; // address of the instruction after the last cond.branch
uint32_t numStatCondBranchInst; // number of static cond. branch instructions up until now (-> unique id for the cond. branch)
//UINT32 lastBrId; // index of last cond. branch instruction
uint64_t* transition_counts;
char* local_taken;
uint64_t* local_taken_counts;
uint64_t* local_brCounts;
ADDRINT* indices_condBr;
uint32_t indices_condBr_size;
/* incorrect predictions counters */
uint64_t GAg_incorrect_pred[NUM_HIST_LENGTHS];
uint64_t GAs_incorrect_pred[NUM_HIST_LENGTHS];
uint64_t PAg_incorrect_pred[NUM_HIST_LENGTHS];
uint64_t PAs_incorrect_pred[NUM_HIST_LENGTHS];
/* prediction for each of the 4 predictors */
uint32_t GAg_pred_taken[NUM_HIST_LENGTHS];
uint32_t GAs_pred_taken[NUM_HIST_LENGTHS];
uint32_t PAg_pred_taken[NUM_HIST_LENGTHS];
uint32_t PAs_pred_taken[NUM_HIST_LENGTHS];
/* size of local pattern history */
uint64_t brHist_size;
/* global/local history */
uint32_t bhr;
uint32_t* local_bhr;
/* global/local pattern history tables */
char*** GAg_pht;
char*** PAg_pht;
char**** GAs_pht;
char**** PAs_pht;
/* check if page entries were touched (memory efficiency) */
char* GAs_touched;
char* PAs_touched;
/* prediction history */
int GAg_pred_hist[NUM_HIST_LENGTHS];
int PAg_pred_hist[NUM_HIST_LENGTHS];
int GAs_pred_hist[NUM_HIST_LENGTHS];
int PAs_pred_hist[NUM_HIST_LENGTHS];

/* initializing */
void branch_init(){

	uint32_t i,j;
	int k;

	brHist_size = 512;

	numStatCondBranchInst = 1;

	/* translation of instruction address to indices */
	indices_condBr_size = 1024;
	indices_condBr = (ADDRINT*) checked_malloc(indices_condBr_size*sizeof(ADDRINT));

	lastInstBr = false;

	/* global/local history */
	bhr = 0;
	local_bhr = (uint32_t*)checked_malloc(brHist_size * sizeof(uint32_t));

	/* GAg PPM predictor */
	GAg_pht = (char***) checked_malloc(NUM_HIST_LENGTHS * sizeof(char**));
	for(j = 0; j < NUM_HIST_LENGTHS; j++) {
		GAg_pht[j] = (char**) checked_malloc((history_lengths[j]+1)*sizeof(char*));
		for(i = 0; i <= history_lengths[j]; i++){
			GAg_pht[j][i] = (char*) checked_malloc((1 << i)*sizeof(char));
			for(k = 0; k < (1 << i); k++)
				GAg_pht[j][i][k] = 0;
		}
	}

	/* PAg PPM predictor */
	PAg_pht = (char***) checked_malloc(NUM_HIST_LENGTHS * sizeof(char**));
	for(j = 0; j < NUM_HIST_LENGTHS; j++) {
		PAg_pht[j] = (char**) checked_malloc((history_lengths[j]+1)*sizeof(char*));
		for(i = 0; i <= history_lengths[j]; i++){
			PAg_pht[j][i] = (char*) checked_malloc((1 << i)*sizeof(char));
			for(k = 0; k < (1 << i); k++)
				PAg_pht[j][i][k] = 0;
		}
	}

	/* GAs PPM predictor */
	GAs_touched = (char*) checked_malloc(brHist_size * sizeof(char));
	GAs_pht = (char****) checked_malloc(brHist_size * sizeof(char***));

	/* PAs PPM predictor */
	PAs_touched = (char*) checked_malloc(brHist_size * sizeof(char));
	PAs_pht = (char****) checked_malloc(brHist_size * sizeof(char***));

	transition_counts = (uint64_t*) checked_malloc(brHist_size * sizeof(uint64_t));
	local_taken = (char*) checked_malloc(brHist_size * sizeof(char));
	local_brCounts = (uint64_t*) checked_malloc(brHist_size * sizeof(uint64_t));
	local_taken_counts = (uint64_t*) checked_malloc(brHist_size * sizeof(uint64_t));

	for(i = 0; i < brHist_size; i++){
		transition_counts[i] = 0;
		local_taken[i] = -1;
		local_brCounts[i] = 0;
		local_taken_counts[i] = 0;
		GAs_touched[i] = 0;
		PAs_touched[i] = 0;
	}

	for(j=0; j < NUM_HIST_LENGTHS; j++){
		GAg_incorrect_pred[j] = 0;
		GAs_incorrect_pred[j] = 0;
		PAg_incorrect_pred[j] = 0;
		PAs_incorrect_pred[j] = 0;
	}
}

/* double memory space for branch history size when needed */
void reallocate_brHist(){

	uint32_t* int_ptr;
	char* char_ptr;
	char**** char4_ptr;
	uint64_t* int64_ptr;

	brHist_size = brHist_size*2;

	int_ptr = (uint32_t*) checked_realloc(local_bhr,brHist_size * sizeof(uint32_t));

	local_bhr = int_ptr;

	char_ptr = (char*) checked_realloc(GAs_touched, brHist_size * sizeof(char));

	GAs_touched = char_ptr;

	char4_ptr = (char****) checked_realloc(GAs_pht,brHist_size * sizeof(char***));

	GAs_pht = char4_ptr;

	char_ptr = (char*) checked_realloc(PAs_touched,brHist_size * sizeof(char));

	PAs_touched = char_ptr;

	char4_ptr = (char****) checked_realloc(PAs_pht,brHist_size * sizeof(char***));

	PAs_pht = char4_ptr;

	char_ptr = (char*) checked_realloc(local_taken,brHist_size * sizeof(char));

	local_taken = char_ptr;

	int64_ptr = (uint64_t*) realloc(transition_counts, brHist_size * sizeof(uint64_t));

	transition_counts = int64_ptr;

	int64_ptr = (uint64_t*) realloc(local_brCounts, brHist_size * sizeof(uint64_t));

	local_brCounts = int64_ptr;

	int64_ptr = (uint64_t*) realloc(local_taken_counts, brHist_size * sizeof(uint64_t));

	local_taken_counts = int64_ptr;
}


void condBr(uint32_t id, bool _t){

	int i,j,k;
	int hist;
	bool taken = (_t != 0) ? 1 : 0;

	/* predict direction */

	/* GAs PPM predictor lookup */
	if(!GAs_touched[id]){
		/* allocate PPM predictor */

		GAs_touched[id] = 1;
        // history_lengths = {4,8,12}
		GAs_pht[id] = (char***) checked_malloc(NUM_HIST_LENGTHS * sizeof(char**));
		for(j = 0; j < NUM_HIST_LENGTHS; j++){
			GAs_pht[id][j] = (char**) checked_malloc((history_lengths[j]+1) * sizeof(char*));
			for(i = 0; i <= (int)history_lengths[j]; i++){
				GAs_pht[id][j][i] = (char*) checked_malloc((1 << i) * sizeof(char));
				for(k = 0; k < (1<<i); k++){
					GAs_pht[id][j][i][k] = -1;
				}
			}
		}
	}

	/* PAs PPM predictor lookup */
	if(!PAs_touched[id]){
		/* allocate PPM predictor */

		PAs_touched[id] = 1;

		PAs_pht[id] = (char***) checked_malloc(NUM_HIST_LENGTHS * sizeof(char**));
		for(j = 0; j < NUM_HIST_LENGTHS; j++){
			PAs_pht[id][j] = (char**) checked_malloc((history_lengths[j]+1) * sizeof(char*));
			for(i = 0; i <= (int)history_lengths[j]; i++){
				PAs_pht[id][j][i] = (char*) checked_malloc((1 << i) * sizeof(char));
				for(k = 0; k < (1 << i); k++){
					PAs_pht[id][j][i][k] = -1;
				}
			}
		}
	}

	for(j = 0; j < NUM_HIST_LENGTHS; j++){
		/* GAg PPM predictor lookup */
		for(i = (int)history_lengths[j]; i >= 0; i--){

			hist = bhr & (((int) 1 << i) -1);
			if(GAg_pht[j][i][hist] != 0){
				GAg_pred_hist[j] = i; // used to only update predictor doing the prediction and higher order predictors (update exclusion)
				if(GAg_pht[j][i][hist] > 0)
					GAg_pred_taken[j] = 1;
				else
					GAg_pred_taken[j] = 0;
				break;
			}
		}

		/* PAg PPM predictor lookup */
		for(i = (int)history_lengths[j]; i >= 0; i--){
			hist = local_bhr[id] & (((int) 1 << i) -1);
			if(PAg_pht[j][i][hist] != 0){
				PAg_pred_hist[j] = i;
				if(PAg_pht[j][i][hist] > 0)
					PAg_pred_taken[j] = 1;
				else
					PAg_pred_taken[j] = 0;
				break;
			}
		}

		/* GAs PPM predictor lookup */
		for(i = (int)history_lengths[j]; i >= 0; i--){
			hist = bhr & (((int) 1 << i) -1);
			if(GAs_pht[id][j][i][hist] != 0){
				GAs_pred_hist[j] = i;
				if(GAs_pht[id][j][i][hist] > 0)
					GAs_pred_taken[j] = 1;
				else
					GAs_pred_taken[j] = 0;
				break;
			}
		}

		/* PAs PPM predictor lookup */
		for(i = (int)history_lengths[j]; i >= 0; i--){
			hist = local_bhr[id] & (((int) 1 << i) -1);
			if(PAs_pht[id][j][i][hist] != 0){
				PAs_pred_hist[j] = i;
				if(PAs_pht[id][j][i][hist] > 0)
					PAs_pred_taken[j] = 1;
				else
					PAs_pred_taken[j] = 0;
				break;
			}
		}
	}

	/* transition/taken rate */
	if(local_taken[id] > -1){
		if(taken != local_taken[id])
			transition_counts[id]++;
	}
	local_taken[id] = taken;
	local_brCounts[id]++;
	if(taken)
		local_taken_counts[id]++;

	for(j=0; j < NUM_HIST_LENGTHS; j++){
		/* update statistics according to predictions */
		if(taken != GAg_pred_taken[j])
			GAg_incorrect_pred[j]++;
		if(taken != GAs_pred_taken[j])
			GAs_incorrect_pred[j]++;
		if(taken != PAg_pred_taken[j])
			PAg_incorrect_pred[j]++;
		if(taken != PAs_pred_taken[j])
			PAs_incorrect_pred[j]++;

		/* using update exclusion: only update predictor doing the prediction and higher order predictors */

		/* update GAg PPM pattern history tables */
		for(i = (int)GAg_pred_hist[j]; i <= (int)history_lengths[j]; i++){
			hist = bhr & ((1 << i) - 1);
			if(taken){
				if(GAg_pht[j][i][hist] < 127)
					GAg_pht[j][i][hist]++;
			}
			else{
				if(GAg_pht[j][i][hist] > -127)
					GAg_pht[j][i][hist]--;
			}
			/* avoid == 0 because that means 'not set' */
			if(GAg_pht[j][i][hist] == 0){
				if(taken){
					GAg_pht[j][i][hist]++;
				}
				else{
					GAg_pht[j][i][hist]--;
				}
			}
		}
		/* update PAg PPM pattern history tables */
		for(i = (int)PAg_pred_hist[j]; i <= (int)history_lengths[j]; i++){
			hist = local_bhr[id] & ((1 << i) - 1);
			if(taken){
				if(PAg_pht[j][i][hist] < 127)
					PAg_pht[j][i][hist]++;
			}
			else{
				if(PAg_pht[j][i][hist] > -127)
					PAg_pht[j][i][hist]--;
			}
			/* avoid == 0 because that means 'not set' */
			if(PAg_pht[j][i][hist] == 0){
				if(taken){
					PAg_pht[j][i][hist]++;
				}
				else{
					PAg_pht[j][i][hist]--;
				}
			}
		}
		/* update GAs PPM pattern history tables */
		for(i = (int)GAs_pred_hist[j]; i <= (int)history_lengths[j]; i++){
			hist = bhr & ((1 << i) - 1);
			if(taken){
				if(GAs_pht[id][j][i][hist] < 127)
					GAs_pht[id][j][i][hist]++;
			}
			else{
				if(GAs_pht[id][j][i][hist] > -127)
					GAs_pht[id][j][i][hist]--;
			}
			/* avoid == 0 because that means 'not set' */
			if(GAs_pht[id][j][i][hist] == 0){
				if(taken){
					GAs_pht[id][j][i][hist]++;
				}
				else{
					GAs_pht[id][j][i][hist]--;
				}
			}
		}
		/* update PAs PPM pattern history tables */
		for(i = (int)PAs_pred_hist[j]; i <= (int)history_lengths[j]; i++){
			hist = local_bhr[id] & ((1 << i) - 1);
			if(taken){
				if(PAs_pht[id][j][i][hist] < 127)
					PAs_pht[id][j][i][hist]++;
			}
			else{
				if(PAs_pht[id][j][i][hist] > -127)
					PAs_pht[id][j][i][hist]--;
			}
			/* avoid == 0 because that means 'not set' */
			if(PAs_pht[id][j][i][hist] == 0){
				if(taken){
					PAs_pht[id][j][i][hist]++;
				}
				else{
					PAs_pht[id][j][i][hist]--;
				}
			}
		}
	}

	/* update global history register */
	bhr = bhr << 1;
	bhr |= taken;

	/* update local history */
	local_bhr[id] = local_bhr[id] << 1;
	local_bhr[id] |= taken;
}

/* index for static conditional branch */
uint32_t index_condBr(ADDRINT ins_addr){

	uint64_t i;
	for(i=0; i <= numStatCondBranchInst; i++){
		if(indices_condBr[i] == ins_addr)
			return i; /* found */
	}
	return 0; /* not found */
}

/* register static conditional branch with some index */
void register_condBr(ADDRINT ins_addr){

	ADDRINT* ptr;

	/* reallocation needed */
	if(numStatCondBranchInst >= indices_condBr_size){

		indices_condBr_size *= 2;
		ptr = (ADDRINT*) realloc(indices_condBr, indices_condBr_size*sizeof(ADDRINT));
		/*if(ptr == (ADDRINT*)NULL){
			cerr << "Could not allocate memory (realloc in register_condBr)!" << endl;
			exit(1);
		}*/
		indices_condBr = ptr;

	}

	/* register instruction to index */
	indices_condBr[numStatCondBranchInst++] = ins_addr;
}

//static int _count  = 0
void instrument_ppm_cond_br(ADDRINT ins_addr,bool taken){
    uint32_t index = index_condBr(ins_addr);
	if(index < 1){

		/* We don't know the number of static conditional branch instructions up front,
		 * so we double the size of the branch history tables as needed by calling this function */
		if(numStatCondBranchInst >= brHist_size)
			reallocate_brHist();

		index = numStatCondBranchInst;

		register_condBr(ins_addr);
        //register_condBr(INS_Address(ins));
	}
    condBr(index,taken);
    
}

/* finishing... */
void branch_exit(FILE* file){

	uint32_t i;
    //fprintf(file,"history length = x :    GAg_incorrect_pred  PAg_incorrect_pred  GAs_incorrect_pred  PAs_incorrect_pred\n");
    fprintf(file,"\n---------------------------------------------------\n");
      fprintf(file,"                        GAg_incorrect_pred  PAg_incorrect_pred  GAs_incorrect_pred  PAs_incorrect_pred\n");
	for(i=0; i < NUM_HIST_LENGTHS; i++){
        fprintf(file,"history length = %d :    %-20lu%-20lu%-20lu%-20lu\n\n",i,GAg_incorrect_pred[i],PAg_incorrect_pred[i],GAs_incorrect_pred[i],PAs_incorrect_pred[i]);
        
    }
		// output_file_ppm << GAg_incorrect_pred[i] << " " << PAg_incorrect_pred[i] << " " << GAs_incorrect_pred[i] << " " << PAs_incorrect_pred[i] << " ";

	uint64_t total_transition_count = 0;
	uint64_t total_taken_count = 0;
	uint64_t total_brCount = 0;
	for(i=0; i < brHist_size; i++){
		if(local_brCounts[i] > 0){
			if( transition_counts[i] > local_brCounts[i]/2)
				total_transition_count += local_brCounts[i]-transition_counts[i];
			else
				total_transition_count += transition_counts[i];

			if( local_taken_counts[i] > local_brCounts[i]/2)
				total_taken_count += local_brCounts[i] - local_taken_counts[i];
			else
				total_taken_count += local_taken_counts[i];
			total_brCount += local_brCounts[i];
		}
	}
    fprintf(file,"total conditional branch counts : %lu\n\n",total_brCount);
    fprintf(file,"total transition count : %lu\n\n",total_transition_count);
    fprintf(file,"total taken count : %lu\n\n",total_transition_count);

	//output_file_ppm << total_brCount << " " << total_transition_count << " " << total_taken_count << endl;
}
