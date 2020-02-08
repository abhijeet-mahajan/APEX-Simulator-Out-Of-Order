//
// Created by Abhijeet Mahajan on 12/8/18.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "cpu.h"



/*
 * Physical Register
 * */
//Check if this value is INT_MIN - Invalid in the case of no free physical register;
int getNextFreePhysicalRegister(APEX_CPU* cpu){
    for(int i=0;i<40;i++){
        if(cpu->allocationList[i] == 0){
            return i;
        }
    }
    return -1;
//    return getRegister(cpu->freePhysicalRegister);
}

int getPhysicalFromArchitecture(APEX_CPU* cpu,int arch){
    return cpu->renameTable[arch];
}

void addRenameTableEntry(APEX_CPU* cpu,int arch, int physical){
    cpu->renameTable[arch]=physical;
    cpu->allocationList[physical]=1;
}
void addBackendRATEntry(APEX_CPU* cpu,int arch, int physical){
    if(cpu->backendRAT[arch] != -1){
        cpu->allocationList[cpu->backendRAT[arch]]=0;
    }
    cpu->backendRAT[arch]=physical;
}




/*
 * Register Queue
 * */


int isRegisterQueueFull(struct PhysicalRegistersQueue* queue)
{
    return (queue->size == queue->capacity);
}

int isRegisterQueueEmpty(struct PhysicalRegistersQueue* queue)
{  return (queue->size == 0); }

void insertRegister(struct PhysicalRegistersQueue* queue, int item) {
    if (isRegisterQueueFull(queue)){
        return;
    }
    queue->rear = (queue->rear + 1)%queue->capacity;
    queue->array[queue->rear] = item;
    queue->size = queue->size + 1;
}

int getRegister(struct PhysicalRegistersQueue* queue) {
    if (isRegisterQueueEmpty(queue))
        return -1;
    int item = queue->array[queue->front];
    queue->front = (queue->front + 1)%queue->capacity;
    queue->size = queue->size - 1;
    return item;
}

int registerQueueFront(struct PhysicalRegistersQueue* queue) {
    if (isRegisterQueueEmpty(queue))
        return -1;
    return queue->array[queue->front];
}

int registerQueueRear(struct PhysicalRegistersQueue* queue) {
    if (isRegisterQueueEmpty(queue))
        return -1;
    return queue->array[queue->rear];
}





/*
 * Instructions - General
 * */


bool isInstructionBZ(char* opcode){
    if (strcmp(opcode,"BZ")==0) return true;
    return false;
}
bool isInstructionBNZ(char* opcode){
    if (strcmp(opcode,"BNZ")==0) return true;
    return false;
}
bool isInstructionJUMP(char* opcode){
    if (strcmp(opcode,"JUMP")==0) return true;
    return false;
}
bool isInstructionJAL(char* opcode){
    if (strcmp(opcode,"JAL")==0) return true;
    return false;
}


bool isInstructionADDL(char* opcode){
    if (strcmp(opcode,"ADDL")==0) return true;
    return false;
}
bool isInstructionSUBL(char* opcode){
    if (strcmp(opcode,"SUBL")==0) return true;
    return false;
}

bool isInstructionHalt(char * opcode){
    if (strcmp(opcode,"HALT")==0) return true;
    return false;
}
bool isInstructionADD(char* opcode){
    if (strcmp(opcode,"ADD")==0) return true;
    return false;
}
bool isInstructionMOVC(char* opcode){
    if (strcmp(opcode,"MOVC")==0) return true;
    return false;
}

bool isInstructionSUB(char* opcode){
    if (strcmp(opcode,"SUB")==0) return true;
    return false;
}
bool isInstructionAND(char* opcode){
    if (strcmp(opcode,"AND")==0) return true;
    return false;
}
bool isInstructionOR(char* opcode){
    if (strcmp(opcode,"OR")==0) return true;
    return false;
}
bool isInstructionEXOR(char* opcode){
    if (strcmp(opcode,"EX-OR")==0) return true;
    return false;
}
bool isInstructionMUL(char* opcode){
    if (strcmp(opcode,"MUL")==0) return true;
    return false;
}

bool isInstructionLoad(char* opcode){
    if (strcmp(opcode,"LOAD")==0) return true;
    return false;
}
bool isInstructionStore(char* opcode){
    if (strcmp(opcode,"STORE")==0) return true;
    return false;
}




/*
 * Issue Queue
 * */

void pushToIssueQueue(APEX_CPU* cpu,CPU_Stage instruction){
    for(int i=0;i<16;i++){
        if(strcmp(cpu->issueQueue[i].opcode,"") == 0){
            cpu->issueQueue[i]=instruction;
            return;
        }
    }
};

bool isIssueQueueFull(APEX_CPU* cpu){
    for(int i=0;i<16;i++){
        if(strcmp(cpu->issueQueue[i].opcode,"")==0) {
            return false;
        }
    }
    return  true;
}

bool isIssueQueueEmpty(APEX_CPU* cpu){
//  for(int i=0;i<16;i++){
//    if(strcmp(cpu->issueQueue[i].opcode,"")!=0) {
//      return false;
//    }
//  }


    //Or you could just check if first entry is null;
    if(strcmp(cpu->issueQueue[0].opcode,"")==0) {
        return  true;
    }
    return false;
}




void makeRegisterInvalid(APEX_CPU* cpu,int reg){
    cpu->physicalRegisters[reg].isValid=0;

}

void makeRegisterValid(APEX_CPU* cpu,int reg ){
    cpu->physicalRegisters[reg].isValid=1;
}

bool isRegisterValid(APEX_CPU* cpu,int reg){
    return (cpu->physicalRegisters[reg].isValid != 0);
}


//ADD,SUB,AND,OR,EX-OR
//bool isArithmaticInstruction(APEX_CPU* cpu, char* opcode){
//    return (isInstructionADD(opcode));
//}


//ADD,SUB,AND,OR,EXOR,LOAD,STORE,
void fetchNextIntegerInstructionFromIssueQueue(APEX_CPU* cpu){
    CPU_Stage stage;
    stage.busy=1;
    for(int i=0;i<16;i++){
        //printf("inside fetchIntInstruction -> i - %s\n",cpu->issueQueue[i].opcode);
        //printf("instr -> %s",cpu->issueQueue[i].opcode);
        if(strcmp(cpu->issueQueue[i].opcode,"")==0){
            cpu->stage[INT_FU]=stage;
            break;
        }else if(isInstructionADD(cpu->issueQueue[i].opcode) ||isInstructionSUB(cpu->issueQueue[i].opcode)
                 || isInstructionAND(cpu->issueQueue[i].opcode) || isInstructionOR(cpu->issueQueue[i].opcode) || isInstructionEXOR(cpu->issueQueue[i].opcode) )
        {
//            printf("Found the instruction %s %d %d \n",cpu->issueQueue[i].opcode,cpu->issueQueue[i].rs1,cpu->issueQueue[i].rs2);
            if(isRegisterValid(cpu,cpu->issueQueue[i].rs1) && isRegisterValid(cpu,cpu->issueQueue[i].rs2)){
                stage=cpu->issueQueue[i];
                cpu->stage[INT_FU]=stage;

                for(int j=i;j<15;j++){
                    cpu->issueQueue[j]=cpu->issueQueue[j+1];
                }
                strcpy(cpu->issueQueue[15].opcode,"");
                cpu->issueQueue[15].rs1=-1;
                cpu->issueQueue[15].rs2=-1;
                cpu->issueQueue[15].rd=-1;

//                printf("About to break\n");
                break;
                return;
//                return stage;
            }
        } else if(isInstructionLoad(cpu->issueQueue[i].opcode)){
            if(isRegisterValid(cpu,cpu->issueQueue[i].rs1)){
                stage=cpu->issueQueue[i];
                cpu->stage[INT_FU]=stage;

                for(int j=i;j<15;j++){
                    cpu->issueQueue[j]=cpu->issueQueue[j+1];
                }
                strcpy(cpu->issueQueue[15].opcode,"");
                cpu->issueQueue[15].rs1=-1;
                cpu->issueQueue[15].rd=-1;

                printf("About to break\n");
                break;


            }
        }else if(isInstructionStore(cpu->issueQueue[i].opcode)){
            //Checking for just Source 2 - Verify this.
            if(isRegisterValid(cpu,cpu->issueQueue[i].rs2)){
                stage=cpu->issueQueue[i];
                cpu->stage[INT_FU]=stage;

                for(int j=i;j<15;j++){
                    cpu->issueQueue[j]=cpu->issueQueue[j+1];
                }
                strcpy(cpu->issueQueue[15].opcode,"");
                cpu->issueQueue[15].rs1=-1;
                cpu->issueQueue[15].rd=-1;

                printf("About to break\n");
                break;
            }


        }else if(isInstructionMOVC(cpu->issueQueue[i].opcode)){
            stage=cpu->issueQueue[i];
            cpu->stage[INT_FU]=stage;

            for(int j=i;j<15;j++){
                cpu->issueQueue[j]=cpu->issueQueue[j+1];
            }
            strcpy(cpu->issueQueue[15].opcode,"");
            cpu->issueQueue[15].rs1=-1;
            cpu->issueQueue[15].rs2=-1;
            cpu->issueQueue[15].rd=-1;

            break;
        }else if(isInstructionADDL(cpu->issueQueue[i].opcode) || isInstructionSUBL(cpu->issueQueue[i].opcode)) {

            if (isRegisterValid(cpu, cpu->issueQueue[i].rs1)) {
                stage = cpu->issueQueue[i];
                cpu->stage[INT_FU] = stage;

                for (int j = i; j < 15; j++) {
                    cpu->issueQueue[j] = cpu->issueQueue[j + 1];
                }
                strcpy(cpu->issueQueue[15].opcode, "");
                cpu->issueQueue[15].rs1 = -1;
                cpu->issueQueue[15].rs2 = -1;
                cpu->issueQueue[15].rd = -1;

                break;
            }
        } else if(isInstructionBZ(cpu->issueQueue[i].opcode)){
            stage=cpu->issueQueue[i];
            cpu->stage[INT_FU]=stage;
            for(int j=i;j<15;j++){
                cpu->issueQueue[j]=cpu->issueQueue[j+1];
            }
            strcpy(cpu->issueQueue[15].opcode,"");
            cpu->issueQueue[15].rs1=-1;
            cpu->issueQueue[15].rs2=-1;
            cpu->issueQueue[15].rd=-1;

            break;
        } else if(isInstructionBNZ(cpu->issueQueue[i].opcode)){
            stage=cpu->issueQueue[i];
            cpu->stage[INT_FU]=stage;

            for(int j=i;j<15;j++){
                cpu->issueQueue[j]=cpu->issueQueue[j+1];
            }
            strcpy(cpu->issueQueue[15].opcode,"");
            cpu->issueQueue[15].rs1=-1;
            cpu->issueQueue[15].rs2=-1;
            cpu->issueQueue[15].rd=-1;

            break;
        } else if(isInstructionJUMP(cpu->issueQueue[i].opcode)){
            stage=cpu->issueQueue[i];
            cpu->stage[INT_FU]=stage;

            for(int j=i;j<15;j++){
                cpu->issueQueue[j]=cpu->issueQueue[j+1];
            }
            strcpy(cpu->issueQueue[15].opcode,"");
            cpu->issueQueue[15].rs1=-1;
            cpu->issueQueue[15].rs2=-1;
            cpu->issueQueue[15].rd=-1;

            break;


        }
    }
}

void fetchNextMultiplyInstructionFromIssueQueue(APEX_CPU* cpu){
    if(cpu->stage[MUL_FU].stalled){
        return;
    }
    CPU_Stage stage;
    for(int i=0;i<16;i++){
        if(isInstructionMUL(cpu->issueQueue[i].opcode) && isRegisterValid(cpu,cpu->issueQueue[i].rs1) && isRegisterValid(cpu,cpu->issueQueue[i].rs2)){
            stage=cpu->issueQueue[i];
            cpu->stage[MUL_FU]=stage;

            for(int j=i;j<15;j++){
                cpu->issueQueue[j]=cpu->issueQueue[j+1];
            }
            strcpy(cpu->issueQueue[15].opcode,"");
            cpu->issueQueue[15].rs1=-1;
            cpu->issueQueue[15].rs2=-1;
            cpu->issueQueue[15].rd=-1;

            return;
        }

    }
}

void fetchNextMemoryInstruction(APEX_CPU* cpu){
    if(cpu->stage[MEM].stalled){
//        printf("Memory stage is stalled so skip rest\n");
        return;
    }

    if(cpu->stage[MEM].busy){
//        printf("Memory was busy.made it non-busy\n");
        cpu->stage[MEM].busy=0;
    }

    CPU_Stage* robStage=peekROBHead(cpu->reorderBuffer);
    CPU_Stage lsqStage=peekLSQHead(cpu->loadStoreQueue);

//    printf("fetchnextmemory rob index-%d, lsq index -%d\n",robStage->index,lsqStage.index);
    if(robStage->index == lsqStage.index){
        if(isInstructionLoad(robStage->opcode)){
//            printf("fetchnextmemory %d - %d \n",lsqStage.is_mem_address_valid,lsqStage.mem_address);
//            if(isRegisterValid(cpu,lsqStage.rd)){
            if(lsqStage.is_mem_address_valid){
                cpu->stage[MEM]=lsqStage;
                cpu->stage[MEM].loadStoreCounter=3;
                return;
            }
        }else if(isInstructionStore(robStage->opcode)){
//            printf("fetchnextmemory %d %d- %d %d\n",isRegisterValid(cpu,lsqStage.rs2),isRegisterValid(cpu,lsqStage.rs2),lsqStage.is_mem_address_valid,lsqStage.mem_address);

            if(isRegisterValid(cpu,lsqStage.rs2) && isRegisterValid(cpu,lsqStage.rs1) && lsqStage.is_mem_address_valid){
                cpu->stage[MEM]=lsqStage;
                cpu->stage[MEM].loadStoreCounter=3;
                return;
            }
        }
    }
//    printf("Made Memory Stage is busy \n");
    cpu->stage[MEM].busy=1;
//    strcpy(cpu->stage[MEM].opcode,"");
}






int isROBFull(struct ReorderBuffer* queue){
    return (queue->size == queue->capacity);
}
int isROBEmpty(struct ReorderBuffer* queue){
    return (queue->size == 0);
}
void pushToROB(struct ReorderBuffer* queue, CPU_Stage stage){
    if (isROBFull(queue)){
        return;
    }
    queue->tail = (queue->tail + 1)%queue->capacity;
    queue->array[queue->tail] = stage;
    queue->size = queue->size + 1;

}
//Check if ROB is not empty first.
CPU_Stage* peekROBHead(struct ReorderBuffer* queue){
    CPU_Stage* item = &queue->array[queue->head];
    return item;
}
void deleteROBHead(struct ReorderBuffer* queue){
    queue->head = (queue->head + 1)%queue->capacity;
    queue->size = queue->size - 1;
}

CPU_Stage* peekROBTail(struct ReorderBuffer* queue){
    CPU_Stage* item = &queue->array[queue->tail];
    return item;
}
void deleteROBTail(struct ReorderBuffer* queue){
    queue->tail = (queue->tail + 1)%queue->capacity;
    queue->size = queue->size - 1;
}




//int headOfROB(struct ReorderBuffer* queue){
//
//}
//int tailOfROB(struct ReorderBuffer* queue){
//
//}

int isLSQFull(struct LSQ* queue){
    return (queue->size == queue->capacity);
}
int isLSQEmpty(struct LSQ* queue){
    return (queue->size == 0);
}
void pushToLSQ(struct LSQ* queue, CPU_Stage stage){
    if (isLSQFull(queue)){
        return;
    }
    queue->rear = (queue->rear + 1)%queue->capacity;
    queue->array[queue->rear] = stage;
    queue->size = queue->size + 1;

}
//Check if LSQ is not empty first.
CPU_Stage peekLSQHead(struct LSQ* queue){
    CPU_Stage item = queue->array[queue->front];
    return item;
}

void deleteLSQHead(struct LSQ* queue){
    queue->front = (queue->front + 1)%queue->capacity;
    queue->size = queue->size - 1;
}

CPU_Stage peekLSQTail(struct LSQ* queue){
    CPU_Stage item = queue->array[queue->rear];
    return item;
}

void deleteLSQTail(struct LSQ* queue){
    queue->rear = (queue->rear + 1)%queue->capacity;
    queue->size = queue->size - 1;
}





