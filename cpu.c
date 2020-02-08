#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
//#include "cpu.h"

#include "methods.c"

int ENABLE_DEBUG_MESSAGES=1;
int halt_flag=0;


int instruction_counter=0;

struct PhysicalRegistersQueue* createQueue(unsigned capacity) {

    struct PhysicalRegistersQueue* queue = (struct PhysicalRegistersQueue*) malloc(sizeof(struct PhysicalRegistersQueue));
    queue->capacity = capacity;
    queue->front = queue->size = 0;
    queue->rear = capacity - 1;
    queue->array = (int*) malloc(queue->capacity * sizeof(int));
    return queue;
}

struct ReorderBuffer* createROBQueue(unsigned capacity){
    struct ReorderBuffer* rob = (struct ReorderBuffer*) malloc(sizeof(struct ReorderBuffer));
    rob->capacity = capacity;
    rob->head = rob->size = 0;
    rob->tail = capacity - 1;
    rob->array = (CPU_Stage*) malloc(rob->capacity * sizeof(CPU_Stage));
    return rob;
}

struct LSQ* createLSQQueue(unsigned capacity){
    struct LSQ* lsq = (struct LSQ*) malloc(sizeof(struct LSQ));
    lsq->capacity = capacity;
    lsq->front = lsq->size = 0;
    lsq->rear = capacity - 1;
    lsq->array = (CPU_Stage*) malloc(lsq->capacity * sizeof(CPU_Stage));
    return lsq;
}


APEX_CPU*
APEX_cpu_init(const char* filename,const char* function_code,const char* function_cycles)
{

    if (!filename) {
        return NULL;
    }

    APEX_CPU* cpu = malloc(sizeof(*cpu));
    if (!cpu) {
        return NULL;
    }

    cpu->pc = 4000;
    cpu->function_cycles=atoi(function_cycles);
    cpu->z_flag=0;

    if(strcmp(function_code , "simulate")==0){
        ENABLE_DEBUG_MESSAGES=0;
    }


    memset(cpu->physicalRegisters, 0, sizeof(Register) * 40);
    memset(cpu->architectureRegisters, 0, sizeof(int) * 16);
    memset(cpu->allocationList, 0, sizeof(int) * 40);
    memset(cpu->renameTable, 0, sizeof(int) * 16);
    memset(cpu->backendRAT, 0, sizeof(int) * 16);

    for(int i=0;i<16;i++){
        cpu->backendRAT[i]=-1;
        cpu->renameTable[i]=-1;
    }
    memset(cpu->stage, 0, sizeof(CPU_Stage) * NUM_STAGES);
    memset(cpu->issueQueue, 0, sizeof(APEX_Instruction) * 16);

    printf("Issue queue initialized - %s \n",cpu->issueQueue->opcode);
    printf("Inside the init -> %s\n",cpu->stage[F].opcode);

    memset(cpu->data_memory, 0, sizeof(int) * 4000);

    cpu->freePhysicalRegister=createQueue(16);  //Capacity is equal to maximum number of physical registers;
    cpu->reorderBuffer=createROBQueue(32); //ROB capacity
    cpu->loadStoreQueue=createLSQQueue(20);

    for(int i=10;i<40;i++){
        insertRegister(cpu->freePhysicalRegister,i);
    }

    cpu->code_memory = create_code_memory(filename, &cpu->code_memory_size);

    if (!cpu->code_memory) {
        free(cpu);
        return NULL;
    }

    if (ENABLE_DEBUG_MESSAGES) {
        fprintf(stderr,
                "APEX_CPU : Initialized APEX CPU, loaded %d instructions\n",
                cpu->code_memory_size);
        fprintf(stderr, "APEX_CPU : Printing Code Memory\n");
        printf("%-9s %-9s %-9s %-9s %-9s\n", "opcode", "rd", "rs1", "rs2", "imm");

        for (int i = 0; i < cpu->code_memory_size; ++i) {
            printf("%-9s %-9d %-9d %-9d %-9d\n",
                   cpu->code_memory[i].opcode,
                   cpu->code_memory[i].rd,
                   cpu->code_memory[i].rs1,
                   cpu->code_memory[i].rs2,
                   cpu->code_memory[i].imm);
        }
    }

    for (int i = 1; i < NUM_STAGES; ++i) {
        cpu->stage[i].busy = 1;
    }

    return cpu;
}

void APEX_cpu_stop(APEX_CPU* cpu) {
    free(cpu->code_memory);
    free(cpu);
}

int get_code_index(int pc) {
    return (pc - 4000) / 4;
}

static void printStageWithURF(CPU_Stage* stage){
    if(isInstructionMOVC(stage->opcode)){
        printf("(I%d) %s,R%d,#%d [%s,U%d,#%d]\n",stage->index,stage->opcode,stage->arch_rd,stage->imm,stage->opcode,stage->rd,stage->imm);
    }else if(isInstructionADD(stage->opcode) || isInstructionSUB(stage->opcode) || isInstructionMUL(stage->opcode) || isInstructionAND(stage->opcode)
             || isInstructionOR(stage->opcode) || isInstructionEXOR(stage->opcode)){
        printf("(I%d) %s,R%d,R%d,R%d [%s,U%d,U%d,U%d]\n",stage->index,stage->opcode,stage->arch_rd,stage->arch_rs1,stage->arch_rs2,stage->opcode,stage->rd,stage->rs1,stage->rs2);
    }else if(isInstructionLoad(stage->opcode)){
        printf("(I%d) %s,R%d,R%d,#%d [%s,U%d,U%d,#%d]\n",stage->index,stage->opcode,stage->arch_rd,stage->arch_rs1,stage->imm,stage->opcode,stage->rd,stage->rs1,stage->imm);
    } else if(isInstructionStore(stage->opcode)){
        printf("(I%d) %s,R%d,R%d,#%d [%s,U%d,U%d,#%d]\n",stage->index,stage->opcode,stage->arch_rs1,stage->arch_rs2,stage->imm,stage->opcode,stage->rs1,stage->rs2,stage->imm);
    }else if(isInstructionHalt(stage->opcode)){
        printf("(I%d) HALT\n",stage->index);
    }else if(isInstructionADDL(stage->opcode) || isInstructionSUBL(stage->opcode)){
        printf("(I%d) %s,R%d,R%d,#%d [%s,U%d,U%d,#%d]\n",stage->index,stage->opcode,stage->arch_rd,stage->arch_rs1,stage->imm,stage->opcode,stage->rd,stage->rs1,stage->imm);
    } else if(isInstructionBZ(stage->opcode)){
        printf("(I%d) %s,#%d \n",stage->index,stage->opcode,stage->imm);
    } else if(isInstructionBNZ(stage->opcode)){
        printf("(I%d) %s,#%d \n",stage->index,stage->opcode,stage->imm);
    }else if(isInstructionJUMP(stage->opcode)){
        printf("(I%d) %s,R%d,#%d [%s,U%d,#%d]\n",stage->index,stage->opcode,stage->arch_rd,stage->imm,stage->opcode,stage->rd,stage->imm);
    }else if(isInstructionJAL(stage->opcode)){
        printf("(I%d) %s,R%d,R%d,#%d [%s,U%d,U%d,#%d]\n",stage->index,stage->opcode,stage->arch_rd,stage->arch_rs1,stage->imm,stage->opcode,stage->rd,stage->rs1,stage->imm);
    }

}

static void print_instruction(CPU_Stage* stage) {
    if(isInstructionMOVC(stage->opcode)){
        printf("(I%d) %s R%d,#%d",stage->index,stage->opcode,stage->arch_rd,stage->imm);
    } else if(isInstructionADD(stage->opcode)) {
        printf("(I%d) %s R%d,R%d,R%d",stage->index,stage->opcode,stage->rd,stage->rs1,stage->rs2);
    } else if(isInstructionSUB(stage->opcode)) {
        printf("(I%d) %s R%d,R%d,R%d",stage->index,stage->opcode,stage->rd,stage->rs1,stage->rs2);
    } else if(isInstructionAND(stage->opcode)) {
        printf("(I%d) %s R%d,R%d,R%d",stage->index,stage->opcode,stage->rd,stage->rs1,stage->rs2);
    } else if(isInstructionOR(stage->opcode)) {
        printf("(I%d) %s R%d,R%d,R%d",stage->index,stage->opcode,stage->rd,stage->rs1,stage->rs2);
    } else if(isInstructionEXOR(stage->opcode)) {
        printf("(I%d) %s R%d,R%d,R%d" ,stage->index,stage->opcode,stage->rd,stage->rs1,stage->rs2);
    } else if(isInstructionMUL(stage->opcode)) {
        printf("(I%d) %s R%d,R%d,R%d",stage->index, stage->opcode, stage->rd, stage->rs1, stage->rs2);
    } else if(isInstructionLoad(stage->opcode)){
        printf("(I%d) %s R%d,R%d,R%d",stage->index, stage->opcode, stage->rd, stage->rs1, stage->imm);
    }else if(isInstructionStore(stage->opcode)){
        printf("(I%d) %s R%d,R%d,R%d",stage->index, stage->opcode, stage->rs1, stage->rs2, stage->imm);
    } else if(isInstructionHalt(stage->opcode)){
        printf("(I%d) HALT",stage->index);
    } else if(isInstructionSUBL(stage->opcode) || isInstructionADDL(stage->opcode)){
        printf("(I%d) %s R%d,R%d,#%d",stage->index, stage->opcode, stage->rd, stage->rs1, stage->imm);
    }else if(isInstructionBZ(stage->opcode)){
        printf("(I%d) %s #%d",stage->index, stage->opcode, stage->imm);
    } else if(isInstructionBNZ(stage->opcode)){
        printf("(I%d) %s #%d",stage->index, stage->opcode, stage->imm);
    }else if(isInstructionJUMP(stage->opcode)){
        printf("(I%d) %s R%d,#%d",stage->index, stage->opcode, stage->rd, stage->imm);
    }else if(isInstructionJAL(stage->opcode)){
        printf("(I%d) %s R%d,R%d,#%d",stage->index, stage->opcode, stage->rd, stage->rs1, stage->imm);
    }
}

//static void print_stage_content(char* name, CPU_Stage* stage) {
//  printf("%-15s: pc(%d) ", name, stage->pc);
//  print_instruction(stage);
//  printf("\n");
//}

int fetch(APEX_CPU* cpu) {
    CPU_Stage* stage = &cpu->stage[F];
    if (!stage->busy && !stage->stalled) {
        stage->pc = cpu->pc;

        APEX_Instruction* current_ins = &cpu->code_memory[get_code_index(cpu->pc)];
        strcpy(stage->opcode, current_ins->opcode);

        stage->rd = current_ins->rd;
        stage->arch_rd=current_ins->rd;

        stage->rs1 = current_ins->rs1;
        stage->arch_rs1=current_ins->rs1;

        stage->rs2 = current_ins->rs2;
        stage->arch_rs2=current_ins->rs2;

        stage->imm = current_ins->imm;
//    stage->rd = current_ins->rd;

        stage->index=instruction_counter++;

        cpu->pc += 4;

        if(!cpu->stage[DRF].stalled){
            cpu->stage[DRF] = cpu->stage[F];
        } else{
            cpu->stage[F].stalled=1;
        }

        if (ENABLE_DEBUG_MESSAGES) {
            printf("01. Instruction at FETCH______STAGE ---> \t");
            print_instruction(stage);
            printf("\n");
//      print_stage_content("Fetch", stage);
        }
    } else if(stage->stalled){

        if(!cpu->stage[DRF].stalled){
            stage->stalled=0;
            cpu->stage[DRF] = cpu->stage[F];
        } else{
            cpu->stage[F].stalled=1;
        }

        printf("01. Instruction at FETCH______STAGE ---> \t");
        print_instruction(stage);
        printf("\n");


    }
    return 0;
}

int decode(APEX_CPU* cpu) {
    CPU_Stage* stage = &cpu->stage[DRF];
    if (!stage->busy && !stage->stalled) {

        if(isInstructionMOVC(stage->opcode)) {
            //replaced with dest with new physical regs
            //printContentsOfRenameTable(cpu);
            int freePhysicalRegister=getNextFreePhysicalRegister(cpu);
            addRenameTableEntry(cpu,stage->rd,freePhysicalRegister);
            stage->rd=freePhysicalRegister;
            makeRegisterInvalid(cpu,stage->rd);
        }
        else if(isInstructionADD(stage->opcode) || isInstructionSUB(stage->opcode) || isInstructionMUL(stage->opcode)
                || isInstructionAND(stage->opcode) || isInstructionOR(stage->opcode) || isInstructionEXOR(stage->opcode))
        {
            //getting the value of physical register which is mapped to rs1 architecture register via rename table.
            stage->rs1=getPhysicalFromArchitecture(cpu,stage->rs1);
            if(stage->rs1== -1){
                int rg=getNextFreePhysicalRegister(cpu);
                addRenameTableEntry(cpu,stage->arch_rs1,rg);
                stage->rs1=rg;
            }
            stage->rs1_value=cpu->physicalRegisters[stage->rs1].value;

            //getting the value of physical register which is mapped to rs2 architecture register via rename table.
            stage->rs2=getPhysicalFromArchitecture(cpu,stage->rs2);
            if(stage->rs2== -1){
                //Check if no physical registers are free;
                int rg=getNextFreePhysicalRegister(cpu);
                addRenameTableEntry(cpu,stage->arch_rs2,rg);
                stage->rs2=rg;
            }
            stage->rs2_value=cpu->physicalRegisters[stage->rs2].value;

            int freePhysicalRegister=getNextFreePhysicalRegister(cpu);
            addRenameTableEntry(cpu,stage->rd,freePhysicalRegister);
            stage->rd=freePhysicalRegister;
            makeRegisterInvalid(cpu,stage->rd);
        }
        else if(isInstructionLoad(stage->opcode)){
            //getting the value of physical register which is mapped to rs1 architecture register via rename table.
            stage->rs1=getPhysicalFromArchitecture(cpu,stage->rs1);
            if(stage->rs1== -1){
                int rg=getNextFreePhysicalRegister(cpu);
                addRenameTableEntry(cpu,stage->arch_rs1,rg);
                stage->rs1=rg;
            }
            stage->rs1_value=cpu->physicalRegisters[stage->rs1].value;


            int freePhysicalRegister=getNextFreePhysicalRegister(cpu);
            addRenameTableEntry(cpu,stage->rd,freePhysicalRegister);
            stage->rd=freePhysicalRegister;
            makeRegisterInvalid(cpu,stage->rd);

            stage->loadStoreCounter = 3;
            stage->is_mem_address_valid=0;
        }
        else if(isInstructionStore(stage->opcode)){
            stage->rs1=getPhysicalFromArchitecture(cpu,stage->rs1);
            if(stage->rs1== -1){
                int rg=getNextFreePhysicalRegister(cpu);
                addRenameTableEntry(cpu,stage->arch_rs1,rg);
                stage->rs1=rg;
            }
            stage->rs1_value=cpu->physicalRegisters[stage->rs1].value;

            //getting the value of physical register which is mapped to rs2 architecture register via rename table.
            stage->rs2=getPhysicalFromArchitecture(cpu,stage->rs2);
            if(stage->rs2== -1){
                //Check if no physical registers are free;
                int rg=getNextFreePhysicalRegister(cpu);
                addRenameTableEntry(cpu,stage->arch_rs2,rg);
                stage->rs2=rg;
            }
            stage->rs2_value=cpu->physicalRegisters[stage->rs2].value;

            stage->is_mem_address_valid=0;

        } else if(isInstructionADDL(stage->opcode) || isInstructionSUBL(stage->opcode)){

            stage->rs1=getPhysicalFromArchitecture(cpu,stage->rs1);
            if(stage->rs1== -1){
                int rg=getNextFreePhysicalRegister(cpu);
                addRenameTableEntry(cpu,stage->arch_rs1,rg);
                stage->rs1=rg;
            }
            stage->rs1_value=cpu->physicalRegisters[stage->rs1].value;

            int freePhysicalRegister=getNextFreePhysicalRegister(cpu);
            addRenameTableEntry(cpu,stage->rd,freePhysicalRegister);
            stage->rd=freePhysicalRegister;
            makeRegisterInvalid(cpu,stage->rd);
        }
        else if(isInstructionJUMP(stage->opcode)){

            stage->rs1=getPhysicalFromArchitecture(cpu,stage->rs1);
            if(stage->rs1== -1){
                int rg=getNextFreePhysicalRegister(cpu);
                addRenameTableEntry(cpu,stage->arch_rs1,rg);
                stage->rs1=rg;
            }
            stage->rs1_value=cpu->physicalRegisters[stage->rs1].value;

        }




        //if(Issue queue is not full)
//    printf("IsIssueQueue Full ->   %d\n",isIssueQueueFull(cpu));
        if(!(cpu->stage[DISPATCH].stalled || isIssueQueueFull(cpu))){
            cpu->stage[DISPATCH] = cpu->stage[DRF];
//      pushToIssueQueue(cpu,cpu->stage[DRF]);
        } else{
            cpu->stage[DRF].stalled=1;
            //Stall this Decode stage.
        }

        if (ENABLE_DEBUG_MESSAGES) {
            printf("02. Instruction at DECODE_RF__STAGE ---> \t");
            printStageWithURF(stage);
        }
    } else if(stage->stalled){
        if(!(cpu->stage[DISPATCH].stalled || isIssueQueueFull(cpu))){
            stage->stalled=0;
            cpu->stage[DISPATCH] = cpu->stage[DRF];
        } else{
            cpu->stage[DRF].stalled=1;
        }

        if (ENABLE_DEBUG_MESSAGES) {
            printf("02. Instruction at DECODE_RF__STAGE ---> \t");
            printStageWithURF(stage);
        }

    }

    return 0;
}

int dispatch(APEX_CPU* cpu){
    CPU_Stage* stage = &cpu->stage[DISPATCH];
    if(cpu->stage[DISPATCH].stalled){
        cpu->stage[DISPATCH].stalled=0;
    }

    if (!stage->busy && !stage->stalled) {
        if(strcmp(stage->opcode,"")==0){

        }
        else if(isInstructionHalt(stage->opcode)){
            if(!isROBFull(cpu->reorderBuffer)){
                pushToROB(cpu->reorderBuffer,cpu->stage[DISPATCH]);
            } else{
                cpu->stage[DISPATCH].stalled=1;
                //stall this stage;

            }
        }
        else if((isInstructionLoad(stage->opcode) || isInstructionStore(stage->opcode))){
            if(!(isLSQFull(cpu->loadStoreQueue) || isIssueQueueFull(cpu) || isROBFull(cpu->reorderBuffer))){
//        printf("Inside decode ls Counter %d\n",stage->loadStoreCounter);

                cpu->stage[DISPATCH].loadStoreCounter=3;
                pushToIssueQueue(cpu,cpu->stage[DISPATCH]);
                pushToROB(cpu->reorderBuffer,cpu->stage[DISPATCH]);
                pushToLSQ(cpu->loadStoreQueue,cpu->stage[DISPATCH]);
            } else{
                cpu->stage[DISPATCH].stalled=1;

                //stall all the states.
            }
        }
        else if(!(isIssueQueueFull(cpu) || isROBFull(cpu->reorderBuffer))){
            pushToIssueQueue(cpu,cpu->stage[DISPATCH]);
            pushToROB(cpu->reorderBuffer,cpu->stage[DISPATCH]);
        } else{

            cpu->stage[DISPATCH].stalled=1;
            //Stall this Decode stage.
        }

        if (ENABLE_DEBUG_MESSAGES) {
//      printf("************************\n");
            printf("Details of IQ (Issue Queue) State --\n");
            printContentsOfIssueQueue(cpu);
            printf("--\n");
            printf("Details of ROB state --\n");
            printContentsOfROB(cpu);
            printf("--\n");
            printf("Details of LSQ (Load-Store-Queue) state --\n");
            printContentsOfLSQ(cpu);
            printf("--\n");
            printf("Details of RENAME TABLE (R-RAT) State -- \n");
            printContentsOfBackendRAT(cpu);
            printf("--\n");
            printf("Details of RENAME TABLE (RAT) State --  \n");
            printContentsOfRenameTable(cpu);
            printf("--\n");


        }
    }
    return 0;

}

int int_function_unit(APEX_CPU* cpu){
//    printf("At the start of int fu %s\n",cpu->stage[INT_FU].opcode);
    fetchNextIntegerInstructionFromIssueQueue(cpu);
    CPU_Stage* stage = &cpu->stage[INT_FU];
    if (!stage->busy && !stage->stalled) {
        if(isInstructionMOVC(stage->opcode)){
//      printf("Stage -> imm - %d\n",stage->imm);
            cpu->physicalRegisters[stage->rd].value=stage->imm;
            makeRegisterValid(cpu,stage->rd);
        } else if(isInstructionADD(stage->opcode)){
            stage->rs1_value=cpu->physicalRegisters[stage->rs1].value;
            stage->rs2_value=cpu->physicalRegisters[stage->rs2].value;

            cpu->physicalRegisters[stage->rd].value=stage->rs1_value + stage->rs2_value;
            makeRegisterValid(cpu,stage->rd);

            if(cpu->physicalRegisters[stage->rd].value == 0){
                cpu->z_flag=1;
            }

        } else if(isInstructionSUB(stage->opcode)){
            stage->rs1_value=cpu->physicalRegisters[stage->rs1].value;
            stage->rs2_value=cpu->physicalRegisters[stage->rs2].value;

            cpu->physicalRegisters[stage->rd].value=stage->rs1_value - stage->rs2_value;
            makeRegisterValid(cpu,stage->rd);

            if(cpu->physicalRegisters[stage->rd].value == 0){
                cpu->z_flag=1;
            }

        }else if(isInstructionAND(stage->opcode)){
            stage->rs1_value=cpu->physicalRegisters[stage->rs1].value;
            stage->rs2_value=cpu->physicalRegisters[stage->rs2].value;

            cpu->physicalRegisters[stage->rd].value=stage->rs1_value & stage->rs2_value;
            makeRegisterValid(cpu,stage->rd);
        }else if(isInstructionOR(stage->opcode)){
            stage->rs1_value=cpu->physicalRegisters[stage->rs1].value;
            stage->rs2_value=cpu->physicalRegisters[stage->rs2].value;

            cpu->physicalRegisters[stage->rd].value=stage->rs1_value | stage->rs2_value;
            makeRegisterValid(cpu,stage->rd);
        }else if(isInstructionEXOR(stage->opcode)){
            stage->rs1_value=cpu->physicalRegisters[stage->rs1].value;
            stage->rs2_value=cpu->physicalRegisters[stage->rs2].value;

            cpu->physicalRegisters[stage->rd].value=stage->rs1_value ^ stage->rs2_value;
            makeRegisterValid(cpu,stage->rd);
        }else if(isInstructionLoad(stage->opcode)){
            stage->rs1_value=cpu->physicalRegisters[stage->rs1].value;
            for(int i=0;i<20;i++){
                if(cpu->loadStoreQueue->array[i].index == stage->index){
                    cpu->loadStoreQueue->array[i].is_mem_address_valid=1;
                    cpu->loadStoreQueue->array[i].mem_address=stage->rs1_value+stage->imm;
                    break;
                }
            }
        }else if(isInstructionStore(stage->opcode)){
            stage->rs2_value=cpu->physicalRegisters[stage->rs2].value;
            for(int i=0;i<20;i++){
                if(cpu->loadStoreQueue->array[i].index == stage->index){
                    cpu->loadStoreQueue->array[i].is_mem_address_valid=1;
                    cpu->loadStoreQueue->array[i].mem_address=stage->rs2_value+stage->imm;
                    break;
                }
            }
        } else if(isInstructionADDL(stage->opcode)){

            stage->rs1_value=cpu->physicalRegisters[stage->rs1].value;

            cpu->physicalRegisters[stage->rd].value=stage->rs1_value + stage->imm;
            makeRegisterValid(cpu,stage->rd);

            if(cpu->physicalRegisters[stage->rd].value == 0){
                cpu->z_flag=1;
            }

        } else if(isInstructionSUBL(stage->opcode)){
            stage->rs1_value=cpu->physicalRegisters[stage->rs1].value;

            cpu->physicalRegisters[stage->rd].value=stage->rs1_value - stage->imm;
            makeRegisterValid(cpu,stage->rd);

            if(cpu->physicalRegisters[stage->rd].value == 0){
                cpu->z_flag=1;
            }
        } else if(isInstructionBZ(stage->opcode)){
            if(cpu->z_flag){

//                strcpy(cpu->stage[DISPATCH].opcode,"");
//                printf("Free this up %d \n",cpu->stage[DISPATCH].rd);
//                makeRegisterValid(cpu,cpu->stage[DISPATCH].rd);
//                strcpy(cpu->stage[DRF].opcode,"");

                if(cpu->stage[DISPATCH].index > stage->index){
//                    printf("Insde the int fu bz dispatch if %s archrd-%d rd-%d\n",cpu->stage[DISPATCH].opcode,cpu->stage[DISPATCH].arch_rd,cpu->stage[DISPATCH].rd);
                    strcpy(cpu->stage[DISPATCH].opcode,"");
                    makeRegisterValid(cpu,cpu->stage[DISPATCH].rd);
                }

                if(cpu->stage[DRF].index > stage->index){
//                    printf("Insde the int fu bz drf if\n");
                    strcpy(cpu->stage[DRF].opcode,"");
//                    makeRegisterInvalid(cpu,cpu->stage[DRF].rd);
                }

                if(cpu->stage[F].index > stage->index){
//                    printf("Insde the int fu bz f if\n");
                    strcpy(cpu->stage[F].opcode,"");
                }

                for(int i=0;i<16;i++){
                    if(strcmp(cpu->issueQueue[i].opcode,"") !=0){
                        int currentIndex=cpu->issueQueue[i].index;

                        if(currentIndex < stage->index && currentIndex > instruction_counter){
                            strcpy(cpu->issueQueue[i].opcode,"");

                            if(cpu->issueQueue[i].rd >-1 && cpu->issueQueue[i].rd < 40){
//                                printf("Free the destination due branch taken");
                                cpu->allocationList[cpu->issueQueue[i].rd]=0;
                                makeRegisterValid(cpu,cpu->issueQueue[i].rd);
                            }
                        }
                    }
                }
                //ROB
                printf(" rob tail index %d\n",peekROBTail(cpu->reorderBuffer)->index);
                while (peekROBTail(cpu->reorderBuffer)->index > stage->index){
                    printf("deleted (I%d) from ROB \n",peekROBTail(cpu->reorderBuffer)->index);
                    deleteROBTail(cpu->reorderBuffer);
                }

                while (peekLSQTail(cpu->loadStoreQueue).index > stage->index){
                    printf("deleted (I%d) from LSQ \n",peekLSQTail(cpu->loadStoreQueue).index);
                    deleteLSQTail(cpu->loadStoreQueue);
                }
                cpu->pc=stage->pc + stage->imm;
            }

        } else if(isInstructionBNZ(stage->opcode)){
            if(!cpu->z_flag){

                if(cpu->stage[DISPATCH].index > stage->index){
//                    printf("Insde the int fu bz dispatch if %s archrd-%d rd-%d\n",cpu->stage[DISPATCH].opcode,cpu->stage[DISPATCH].arch_rd,cpu->stage[DISPATCH].rd);
                    strcpy(cpu->stage[DISPATCH].opcode,"");
                    makeRegisterValid(cpu,cpu->stage[DISPATCH].rd);
                }

                if(cpu->stage[DRF].index > stage->index){
//                    printf("Insde the int fu bz drf if\n");
                    strcpy(cpu->stage[DRF].opcode,"");
//                    makeRegisterInvalid(cpu,cpu->stage[DRF].rd);
                }

                if(cpu->stage[F].index > stage->index){
//                    printf("Insde the int fu bz f if\n");
                    strcpy(cpu->stage[F].opcode,"");
                }

                for(int i=0;i<16;i++){
                    if(strcmp(cpu->issueQueue[i].opcode,"") !=0){
                        int currentIndex=cpu->issueQueue[i].index;

                        if(currentIndex < stage->index && currentIndex > instruction_counter){
                            strcpy(cpu->issueQueue[i].opcode,"");

                            if(cpu->issueQueue[i].rd >-1 && cpu->issueQueue[i].rd < 40){
//                                printf("Free the destination due branch taken");
                                cpu->allocationList[cpu->issueQueue[i].rd]=0;
                                makeRegisterValid(cpu,cpu->issueQueue[i].rd);
                            }
                        }
                    }
                }
                //ROB
                printf(" rob tail index %d\n",peekROBTail(cpu->reorderBuffer)->index);
                while (peekROBTail(cpu->reorderBuffer)->index > stage->index){
                    printf("deleted (I%d) from ROB \n",peekROBTail(cpu->reorderBuffer)->index);
                    deleteROBTail(cpu->reorderBuffer);
                }

                while (peekLSQTail(cpu->loadStoreQueue).index > stage->index){
                    printf("deleted (I%d) from LSQ \n",peekLSQTail(cpu->loadStoreQueue).index);
                    deleteLSQTail(cpu->loadStoreQueue);
                }
                cpu->pc=stage->pc + stage->imm;
            }

        } else if(isInstructionJUMP(stage->opcode)) {


            if(cpu->stage[DISPATCH].index > stage->index){
                strcpy(cpu->stage[DISPATCH].opcode,"");
                makeRegisterValid(cpu,cpu->stage[DISPATCH].rd);
            }

            if(cpu->stage[DRF].index > stage->index){
                strcpy(cpu->stage[DRF].opcode,"");
            }

            if(cpu->stage[F].index > stage->index){
                strcpy(cpu->stage[F].opcode,"");
            }

            for(int i=0;i<16;i++){
                if(strcmp(cpu->issueQueue[i].opcode,"") !=0){
                    int currentIndex=cpu->issueQueue[i].index;

                    if(currentIndex < stage->index && currentIndex > instruction_counter){
                        strcpy(cpu->issueQueue[i].opcode,"");

                        if(cpu->issueQueue[i].rd >-1 && cpu->issueQueue[i].rd < 40){
                            cpu->allocationList[cpu->issueQueue[i].rd]=0;
                            makeRegisterValid(cpu,cpu->issueQueue[i].rd);
                        }
                    }
                }
            }
            while (peekROBTail(cpu->reorderBuffer)->index > stage->index){
                printf("deleted (I%d) from ROB \n",peekROBTail(cpu->reorderBuffer)->index);
                deleteROBTail(cpu->reorderBuffer);
            }

            while (peekLSQTail(cpu->loadStoreQueue).index > stage->index){
                printf("deleted (I%d) from LSQ \n",peekLSQTail(cpu->loadStoreQueue).index);
                deleteLSQTail(cpu->loadStoreQueue);
            }

            stage->rs1_value=cpu->physicalRegisters[stage->rs1].value;
            cpu->pc=stage->rs1_value + stage->imm;


            makeRegisterValid(cpu,stage->rd);

            if(cpu->physicalRegisters[stage->rd].value == 0){
                cpu->z_flag=1;
            }

        }


        if (ENABLE_DEBUG_MESSAGES) {
            printf("03. Instruction at EX_INT_FU__STAGE --->\t");
            printStageWithURF(stage);
            printf("--\n");
        }
    }
    return 0;
}

int mul_function_unit(APEX_CPU* cpu){
    fetchNextMultiplyInstructionFromIssueQueue(cpu);
    CPU_Stage* stage = &cpu->stage[MUL_FU];
    if (ENABLE_DEBUG_MESSAGES) {
//        printf("****\n");
        printf("04. Instruction at EX_MUL_FU_STAGE --->\t");
        printStageWithURF(stage);
        printf("--\n");
    }

    if (!stage->busy && !stage->stalled) {
        if(isInstructionMUL(stage->opcode)){
            stage->stalled=1;
        }

        //cpu->stage[WB] = cpu->stage[MEM];
    }else if(stage->stalled && isInstructionMUL(stage->opcode)){
        stage->stalled=0;
        stage->rs1_value=cpu->physicalRegisters[stage->rs1].value;
        stage->rs2_value=cpu->physicalRegisters[stage->rs2].value;

        cpu->physicalRegisters[stage->rd].value=stage->rs1_value * stage->rs2_value;
        makeRegisterValid(cpu,stage->rd);
        strcpy(cpu->stage[MUL_FU].opcode,"");

        if(cpu->physicalRegisters[stage->rd].value == 0){
            cpu->z_flag=1;
        }


    }

    return 0;

}


int memory_function_unit(APEX_CPU* cpu) {
    fetchNextMemoryInstruction(cpu);
    CPU_Stage* stage = &cpu->stage[MEM];
//    printf("opcode - %s,%d,%d,%d imm-%d, ismem-%d, index-%d,ls counter-%d",stage->opcode,stage->rd,stage->rs1,stage->rs2,stage->imm,stage->is_mem_address_valid,stage->index,stage->loadStoreCounter);

    if (!stage->busy && !stage->stalled) {

        if(strcmp(stage->opcode,"")==0){
            deleteLSQHead(cpu->loadStoreQueue);
        }
        else if(isInstructionLoad(stage->opcode)){
//            printf("LOAD STORE count in cycle 1 -> %d\n",stage->loadStoreCounter);
            cpu->stage[MEM].loadStoreCounter = cpu->stage[MEM].loadStoreCounter-1; //3-1=2
            cpu->stage[MEM].stalled=1;
            deleteLSQHead(cpu->loadStoreQueue);

//      cpu->stage[MEM]=stage;

            if (ENABLE_DEBUG_MESSAGES) {
                printf("Details of MEM FU - \n");
                printf(" cycle(%d) - ", 3-(stage->loadStoreCounter));
                printStageWithURF(stage);
                printf("--\n");
            }
        }
        else if(isInstructionStore(stage->opcode)){
//            printf("LOAD STORE count in cycle 1 -> %d\n",stage->loadStoreCounter);
            cpu->stage[MEM].loadStoreCounter = cpu->stage[MEM].loadStoreCounter-1; //3-1=2
            cpu->stage[MEM].stalled=1;

            cpu->data_memory[stage->mem_address] = cpu->physicalRegisters[stage->rs1].value;

            deleteLSQHead(cpu->loadStoreQueue);

            if (ENABLE_DEBUG_MESSAGES) {
                printf("Details of MEM FU - \n");
                printf(" cycle(%d) - ", 3-(stage->loadStoreCounter));
                printStageWithURF(stage);
                printf("--\n");
            }
        }
    }else if(stage->stalled){
        if(isInstructionLoad(stage->opcode)){

            stage->loadStoreCounter = stage->loadStoreCounter-1;

            if (ENABLE_DEBUG_MESSAGES) {
                printf("Details of MEM FU - \n");
                printf(" cycle(%d) - ", 3-(stage->loadStoreCounter));
                printStageWithURF(stage);
                printf("--\n");
            }

            if(stage->loadStoreCounter == 0){
                stage->stalled=0;
                cpu->physicalRegisters[stage->rd].value=cpu->data_memory[stage->mem_address];
//                printf("Stage memory address is -> %d \n",stage->mem_address);
                makeRegisterValid(cpu,stage->rd);
                strcpy(cpu->stage[MEM].opcode,"");
            }

        }
        else if(isInstructionStore(stage->opcode)){
            stage->loadStoreCounter = stage->loadStoreCounter-1;

            if (ENABLE_DEBUG_MESSAGES) {
                printf("Details of MEM FU - \n");
                printf(" cycle(%d) - ", 3-(stage->loadStoreCounter));
                printStageWithURF(stage);
                printf("--\n");
            }

            if(stage->loadStoreCounter == 0){
                stage->stalled=0;
                cpu->data_memory[stage->mem_address] = cpu->physicalRegisters[stage->rs1].value;
//                printf("Stage memory address is -> %d \n",stage->mem_address);
                strcpy(cpu->stage[MEM].opcode,"");
            }

        }
//    stage->loadStoreCounter = stage->loadStoreCounter-1;
//
//    if(stage->loadStoreCounter == 0){
//      if(isInstructionLoad(stage->opcode)) {
//        cpu->physicalRegisters[stage->rd].value = cpu->data_memory[stage->mem_address];
//        makeRegisterValid(cpu, stage->rd);
//        deleteROBHead(cpu->reorderBuffer);
//
//      } else if(isInstructionStore(stage->opcode)){
//        cpu->data_memory[stage->mem_address]=stage->rs1_value;
//      }
//      strcpy(cpu->stage[MEM].opcode,"");
//      cpu->stage[MEM].stalled=0;
//    }
//    if (ENABLE_DEBUG_MESSAGES) {
//      printf("Details of MEM FU - \n");
//      printf(" cycle(%d) - ", 4-(stage->loadStoreCounter));
//      printStageWithURF(stage);
//      printf("--\n");
//    }

    }
    return 0;
}


int commit(APEX_CPU* cpu) {
//  int isRetired=0;
    CPU_Stage* stage=peekROBHead(cpu->reorderBuffer);
//    printf("Whats the ROB head %s,%d,%d,%d",stage->opcode,stage->rd,stage->rs1,stage->rs2);
    if (!stage->busy && !stage->stalled) {
//        printf("Why isn't this working - %s %d %d\n",stage->opcode,cpu->physicalRegisters[stage->rd].isValid,stage->rd);


        if(strcmp(stage->opcode,"")==0){
            deleteROBHead(cpu->reorderBuffer);
        }
        else if(isInstructionMOVC(stage->opcode) && cpu->physicalRegisters[stage->rd].isValid){
            addBackendRATEntry(cpu,stage->arch_rd,stage->rd); //Backend RAT entry

            cpu->architectureRegisters[stage->arch_rd]=cpu->physicalRegisters[stage->rd].value; //Add value to arch register
            deleteROBHead(cpu->reorderBuffer);

            if (ENABLE_DEBUG_MESSAGES) {
                printf("Details of ROB Retired Instructions\n");
                printStageWithURF(stage);
                printf("--\n");
            }

            checkForBackToBackExecution(cpu);


        } else if((isInstructionADD(stage->opcode) || isInstructionSUB(stage->opcode) || isInstructionMUL(stage->opcode)
                   || isInstructionAND(stage->opcode) || isInstructionOR(stage->opcode) || isInstructionEXOR(stage->opcode)
                   || isInstructionSUBL(stage->opcode) || isInstructionADDL(stage->opcode)) && cpu->physicalRegisters[stage->rd].isValid){

            addBackendRATEntry(cpu,stage->arch_rd,stage->rd); //Backend RAT entry
            cpu->architectureRegisters[stage->arch_rd]=cpu->physicalRegisters[stage->rd].value; //Add value to arch register
            deleteROBHead(cpu->reorderBuffer);

            if (ENABLE_DEBUG_MESSAGES) {
                printf("Details of ROB Retired Instructions\n");
                printStageWithURF(stage);
                printf("--\n");

            }

            checkForBackToBackExecution(cpu);
        }
        else if(isInstructionLoad(stage->opcode)){
            if(isRegisterValid(cpu,stage->rd))
            {
//                printf("Commit load -> This should be the third cycle%d \n",stage->loadStoreCounter);
                addBackendRATEntry(cpu,stage->arch_rd,stage->rd); //Backend RAT entry
                cpu->architectureRegisters[stage->arch_rd]=cpu->physicalRegisters[stage->rd].value; //Add value to arch register
                deleteROBHead(cpu->reorderBuffer);

                if (ENABLE_DEBUG_MESSAGES) {
                    printf("Details of ROB Retired Instructions\n");
                    printStageWithURF(stage);
                    printf("--\n");
                }


                checkForBackToBackExecution(cpu);

            }

        } else if(isInstructionStore(stage->opcode)){
            //stage->index ==
            //&& (stage->index == peekLSQHead(cpu->loadStoreQueue).index
//            printf("Commit Store -** load store count - %d\n",cpu->stage[MEM].loadStoreCounter);
//            printf("Commit Store - load store count - %d\n",stage->loadStoreCounter);
            stage->loadStoreCounter =stage->loadStoreCounter -1;
            if(stage->loadStoreCounter == 1) {
//                printf("Inside commit of store  %d \n", stage->loadStoreCounter);
                deleteROBHead(cpu->reorderBuffer);

                if (ENABLE_DEBUG_MESSAGES) {
                    printf("Details of ROB Retired Instructions\n");
                    printStageWithURF(stage);
                    printf("--\n");
                }

                checkForBackToBackExecution(cpu);

            }
        }
        else if(isInstructionHalt(stage->opcode)){
            halt_flag=1;
        }
        else if(isInstructionBZ(stage->opcode)){
            deleteROBHead(cpu->reorderBuffer);
            if (ENABLE_DEBUG_MESSAGES) {
                printf("Details of ROB Retired Instructions\n");
                printStageWithURF(stage);
                printf("--\n");
            }
            checkForBackToBackExecution(cpu);

        }
        else if(isInstructionBNZ(stage->opcode)){
            deleteROBHead(cpu->reorderBuffer);
            if (ENABLE_DEBUG_MESSAGES) {
                printf("Details of ROB Retired Instructions\n");
                printStageWithURF(stage);
                printf("--\n");
            }
            checkForBackToBackExecution(cpu);

        }
        else if(isInstructionJUMP(stage->opcode)){
            deleteROBHead(cpu->reorderBuffer);
            if (ENABLE_DEBUG_MESSAGES) {
                printf("Details of ROB Retired Instructions\n");
                printStageWithURF(stage);
                printf("--\n");
            }
            checkForBackToBackExecution(cpu);

        }
        else if(isInstructionJAL(stage->opcode)){
            deleteROBHead(cpu->reorderBuffer);
            if (ENABLE_DEBUG_MESSAGES) {
                printf("Details of ROB Retired Instructions\n");
                printStageWithURF(stage);
                printf("--\n");
            }
            checkForBackToBackExecution(cpu);

        }



        cpu->ins_completed++;

    }
    return 0;
}

int APEX_cpu_run(APEX_CPU* cpu) {
    int i=0;
//    int initial_PC_Value=cpu->pc;
    while (1) {
        if(cpu->clock ==cpu->function_cycles ){
            break;
        }

//            printf("Code Memory size %d -> \n",cpu->code_memory_size);
//        printf("Ins completed %d -> \n",cpu->ins_completed);
//        printf("CPU pc %d -> \n",cpu->pc);

//    if (cpu->ins_completed == cpu->code_memory_size) {
//      printf("(apex) >> Simulation Complete");
//      break;
//    }

        if (ENABLE_DEBUG_MESSAGES) {
            printf("--------------------------------------------------------------------------------------\n");
            printf("\t\t\t\t\tClock Cycle #: %d\n", cpu->clock);
            printf("--------------------------------------------------------------------------------------\n");
        }

        commit(cpu);

        if(halt_flag){
//      deleteROBHead(cpu->reorderBuffer);
            if(!cpu->stage[MEM].stalled)
                break;
        }


        memory_function_unit(cpu);
        mul_function_unit(cpu);
        int_function_unit(cpu);
        dispatch(cpu);
        decode(cpu);
        fetch(cpu);
        cpu->clock++;
        i++;



//    printContentsOfIssueQueue(cpu);
//    printContentsOfRenameTable(cpu);
//    printContentsOfFreeRegistersList(cpu);
    }


    printf("\n");
    printContentsOfArchitectureRegister(cpu);
    printContentsOfAllocationList(cpu);
    printDataMemory(cpu);

    return 0;
}

//Helper Functions
void printContentsOfIssueQueue(APEX_CPU* cpu){
//    printf("\n\n\n|\tIssue Queue\t|\n\n\n");
    for(int i=0;i<16;i++){
        printStageWithURF(&cpu->issueQueue[i]);
//        printf("%s,%d,%d\n",cpu->issueQueue[i].opcode,cpu->issueQueue[i].rd,cpu->issueQueue[i].imm);
//    if(isInstructionMOVC(cpu->issueQueue[i].opcode)) {
//      printf("IQ[%d] \t -> \t (I%d) %s,R%d,#%d  [%s,U%d,#%d] \n", i,cpu->issueQueue[i].index, cpu->issueQueue[i].opcode,cpu->issueQueue[i].arch_rd,cpu->issueQueue[i].imm,
//             cpu->issueQueue[i].opcode, cpu->issueQueue[i].rd,cpu->issueQueue[i].imm);
//    } else {
//      //printf("%d \t %s \t U%d \t U%d \t U%d\n", i, cpu->issueQueue[i].opcode, cpu->issueQueue[i].rd, cpu->issueQueue[i].rs1,  cpu->issueQueue[i].rs2);
//    }
    }
}

void printContentsOfROB(APEX_CPU* cpu){
    for(int i=cpu->reorderBuffer->head;i<=cpu->reorderBuffer->tail;i++){
        printStageWithURF(&cpu->reorderBuffer->array[i]);
//    if(isInstructionMOVC(cpu->reorderBuffer->array[i].opcode)) {
//      printf("IQ[%d] \t -> \t (I%d) %s,R%d,#%d  [%s,U%d,#%d] \n", i, cpu->reorderBuffer->array[i].index,
//             cpu->reorderBuffer->array[i].opcode, cpu->reorderBuffer->array[i].arch_rd,
//             cpu->reorderBuffer->array[i].imm, cpu->reorderBuffer->array[i].opcode,
//             cpu->reorderBuffer->array[i].rd,
//             cpu->reorderBuffer->array[i].imm);
//    }
    }
}

void printContentsOfLSQ(APEX_CPU* cpu){
//    printf("Inside LSQ %d",cpu->loadStoreQueue->size);
    for(int i = cpu->loadStoreQueue->front;i<=cpu->loadStoreQueue->rear;i++){
//        printf("i -> %d\n",i);

        printStageWithURF(&cpu->loadStoreQueue->array[i]);
//    if(isInstructionMOVC(cpu->loadStoreQueue->array[i].opcode)) {
//      printf("IQ[%d] \t -> \t (I%d) %s,R%d,#%d  [%s,U%d,#%d] \n", i, cpu->loadStoreQueue->array[i].index,
//             cpu->loadStoreQueue->array[i].opcode, cpu->loadStoreQueue->array[i].arch_rd,
//             cpu->loadStoreQueue->array[i].imm, cpu->loadStoreQueue->array[i].opcode,
//             cpu->loadStoreQueue->array[i].rd,
//             cpu->loadStoreQueue->array[i].imm);
//    }
    }

}

void printContentsOfRenameTable(APEX_CPU* cpu){
    for(int i=0;i<16;i++){
        if(cpu->renameTable[i] != -1) {
            printf("RAT[%d] --> U%d\n",i,cpu->renameTable[i]);
        }
    }
}

void printContentsOfBackendRAT(APEX_CPU* cpu){
    for(int i=0;i<16;i++){
        if(cpu->backendRAT[i] != -1)
            printf("RAT[%d] --> U%d\n",i,cpu->backendRAT[i]);
    }
}

void printContentsOfFreeRegistersList(APEX_CPU* cpu){
    printf("\n\n\n|\tFree Registers\t|\n\n\n");
    printf("Head ->\t");
    for(int i=cpu->freePhysicalRegister->front;i<=cpu->freePhysicalRegister->rear;i++){
        printf("%d \t",cpu->freePhysicalRegister->array[i]);
    }
    printf("\n");

}

void printContentsOfArchitectureRegister(APEX_CPU* cpu){
    printf("Architecture Registers -> \n");
    for(int i=0;i<16;i++){
        printf("|[%d]->%d |\t",i,cpu->architectureRegisters[i]);
    }
    printf("\n --\n");
}

void printContentsOfAllocationList(APEX_CPU* cpu){
    printf("\n Allocation List -> \n");

    for(int i=0;i<40;i++){
        if(cpu->allocationList[i] != 0){
            printf(" %d, ",i);
        }
    }
    printf("\n");
}

void printDataMemory(APEX_CPU* cpu){
    printf("\n Data Memory -> \n");

    for(int i=0;i<4096;i++){
        if(cpu->data_memory[i] != 0){
            printf("MEM[%d] - %d\n",i,cpu->data_memory[i]);
        }
    }
    printf("\n");
}


void checkForBackToBackExecution(APEX_CPU* cpu){
    CPU_Stage* stage=peekROBHead(cpu->reorderBuffer);

    if (!stage->busy && !stage->stalled) {
        if(isInstructionMOVC(stage->opcode) && cpu->physicalRegisters[stage->rd].isValid){
            addBackendRATEntry(cpu,stage->arch_rd,stage->rd); //Backend RAT entry

            cpu->architectureRegisters[stage->arch_rd]=cpu->physicalRegisters[stage->rd].value; //Add value to arch register
            deleteROBHead(cpu->reorderBuffer);

            if (ENABLE_DEBUG_MESSAGES) {
                printStageWithURF(stage);
                printf("--\n");
            }
        } else if((isInstructionADD(stage->opcode) || isInstructionSUB(stage->opcode) || isInstructionMUL(stage->opcode)
                   || isInstructionAND(stage->opcode) || isInstructionOR(stage->opcode) || isInstructionEXOR(stage->opcode)
                   || isInstructionSUBL(stage->opcode) || isInstructionADDL(stage->opcode)) && cpu->physicalRegisters[stage->rd].isValid){

            addBackendRATEntry(cpu,stage->arch_rd,stage->rd); //Backend RAT entry
            cpu->architectureRegisters[stage->arch_rd]=cpu->physicalRegisters[stage->rd].value; //Add value to arch register
            deleteROBHead(cpu->reorderBuffer);

            if (ENABLE_DEBUG_MESSAGES) {
                printStageWithURF(stage);
                printf("--\n");

            }
        }
//        else if(isInstructionStore(stage->opcode)){
//            printf("back to back execution Load store counter - %d\n",stage->loadStoreCounter);
//            stage->loadStoreCounter =stage->loadStoreCounter -1;
//
//            if(stage->loadStoreCounter == 1) {
//                deleteROBHead(cpu->reorderBuffer);
//
//                if (ENABLE_DEBUG_MESSAGES) {
//                    printStageWithURF(stage);
//                    printf("--\n");
//                }
//
//            }
//        }
        else if(isInstructionHalt(stage->opcode)){
            halt_flag=1;
        }
        else if(isInstructionBZ(stage->opcode)){
            deleteROBHead(cpu->reorderBuffer);
            if (ENABLE_DEBUG_MESSAGES) {
                printStageWithURF(stage);
                printf("--\n");
            }

        }
        else if(isInstructionBNZ(stage->opcode)){
            deleteROBHead(cpu->reorderBuffer);
            if (ENABLE_DEBUG_MESSAGES) {
                printStageWithURF(stage);
                printf("--\n");
            }

        }
        else if(isInstructionJUMP(stage->opcode)){
            deleteROBHead(cpu->reorderBuffer);
            if (ENABLE_DEBUG_MESSAGES) {
                printStageWithURF(stage);
                printf("--\n");
            }
        }
        else if(isInstructionJAL(stage->opcode)){
            deleteROBHead(cpu->reorderBuffer);
            if (ENABLE_DEBUG_MESSAGES) {
                printStageWithURF(stage);
                printf("--\n");
            }
        }
        cpu->ins_completed++;
    }
}





