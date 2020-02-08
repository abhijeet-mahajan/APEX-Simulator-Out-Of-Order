#include <stdbool.h>

#ifndef _APEX_CPU_H_
#define _APEX_CPU_H_




typedef struct PhysicalRegistersQueue {
    int front;
    int rear;
    int size;
    unsigned capacity;
    int* array;
}PhysicalRegistersQueue;

enum
{
    F,
    DRF,
    DISPATCH,
    INT_FU,
    MUL_FU,
    MEM,
    WB,
    NUM_STAGES
};

typedef struct APEX_Instruction {
    char opcode[128];	// Operation Code
    int rd;		    // Destination Register Address
    int rs1;		    // Source-1 Register Address
    int rs2;		    // Source-2 Register Address
    int imm;		    // Literal Value
} APEX_Instruction;

typedef struct CPU_Stage {
    int index;
    int pc;		    // Program Counter
    char opcode[128];	// Operation Code
    int rs1;		    // Source-1 Register Address
    int is_rs1_valid;//0 - If invalid  1-valid
    int rs2;		    // Source-2 Register Address
    int is_rs2_valid;//0 - If invalid  1-valid
    int rd;		    // Destination Register Address
    int is_rd_valid;    //0 - If invalid  1-valid

    int arch_rs1;
    int arch_rs2;
    int arch_rd;

    int imm;		    // Literal Value
    int rs1_value;	// Source-1 Register Value
    int rs2_value;	// Source-2 Register Value
    int buffer;		// Latch to hold some value
    int mem_address;	// Computed Memory Address
    int is_mem_address_valid;
    int loadStoreCounter;
    int busy;		    // Flag to indicate, stage is performing some action
    int stalled;		// Flag to indicate, stage is stalled
} CPU_Stage;

typedef struct ReorderBuffer {
    int head;
    int tail;
    int size;
    unsigned capacity;
    CPU_Stage* array;
}ReorderBuffer;

typedef struct LSQ{
    int front;
    int rear;
    int size;
    unsigned capacity;
    CPU_Stage* array;
}LSQ;

typedef struct Register {
    int value;
    int zeroFlag;
    int isValid;    //0 - If invalid  1-valid
} Register;


typedef struct APEX_CPU
{
    int clock;
    int pc;

    int architectureRegisters[16];    //architecture register and the value it contains
    Register physicalRegisters[40];    //Physical register and the value it contains
    int allocationList[40];   //1- physical regs is allocated to someone and 0- if physical register is free
    int renameTable[16];   //index- arch register and value is the physical register
    int backendRAT[16];

    PhysicalRegistersQueue* freePhysicalRegister;
    ReorderBuffer* reorderBuffer;
    LSQ* loadStoreQueue;

//  int renamed[32];
//  int status;//status


    int z_flag;
    CPU_Stage stage[7];
    CPU_Stage issueQueue[16];

    APEX_Instruction* code_memory;
    int code_memory_size;
    int data_memory[4096];
    int ins_completed;
    int function_cycles;


    //hate to do this
    CPU_Stage nopStage;

} APEX_CPU;

APEX_Instruction*
create_code_memory(const char* filename, int* size);

APEX_CPU*
APEX_cpu_init(const char* filename,const char* function_name,const char* function_cycles);

int
APEX_cpu_run(APEX_CPU* cpu);

void
APEX_cpu_stop(APEX_CPU* cpu);

int
fetch(APEX_CPU* cpu);

int
decode(APEX_CPU* cpu);

int
dispatch(APEX_CPU* cpu);

int
int_function_unit(APEX_CPU* cpu);

int
mul_function_unit(APEX_CPU* cpu);

int
memory_function_unit(APEX_CPU* cpu);

int
commit(APEX_CPU* cpu);


int getNextFreePhysicalRegister(APEX_CPU* cpu);
int getPhysicalFromArchitecture(APEX_CPU* cpu,int arch);
void addRenameTableEntry(APEX_CPU* cpu,int arch, int physical);

//Physical Register Queue Operations
int isRegisterQueueFull(struct PhysicalRegistersQueue* queue);
int isRegisterQueueEmpty(struct PhysicalRegistersQueue* queue);
void insertRegister(struct PhysicalRegistersQueue* queue, int item);
int getRegister(struct PhysicalRegistersQueue* queue);
int registerQueueFront(struct PhysicalRegistersQueue* queue);
int registerQueueRear(struct PhysicalRegistersQueue* queue);



bool isInstructionMOVC(char* opcode);
bool isInstructionADD(char* opcode);
bool isInstructionSUB(char* opcode);
bool isInstructionMUL(char* opcode);
bool isInstructionAND(char* opcode);
bool isInstructionOR(char* opcode);
bool isInstructionEXOR(char* opcode);
bool isInstructionHalt(char* opcode);

bool isInstructionLoad(char* opcode);
bool isInstructionStore(char* opcode);



bool isIssueQueueFull(APEX_CPU* cpu);
bool isIssueQueueEmpty(APEX_CPU* cpu);
void pushToIssueQueue(APEX_CPU* cpu,CPU_Stage instruction);
void fetchNextIntegerInstructionFromIssueQueue(APEX_CPU* cpu);
void fetchNextMultiplyInstructionFromIssueQueue(APEX_CPU* cpu);
void fetchNextMemoryInstruction(APEX_CPU* cpu);



//Print
void printContentsOfIssueQueue(APEX_CPU* cpu);
void printContentsOfRenameTable(APEX_CPU* cpu);
void printContentsOfBackendRAT(APEX_CPU* cpu);
void printContentsOfFreeRegistersList(APEX_CPU* cpu);
void printContentsOfROB(APEX_CPU* cpu);
void printContentsOfLSQ(APEX_CPU* cpu);
void printContentsOfArchitectureRegister(APEX_CPU* cpu);
void printContentsOfAllocationList(APEX_CPU* cpu);


void makeRegisterInvalid(APEX_CPU* cpu,int reg);
void makeRegisterValid(APEX_CPU* cpu,int reg );
bool isRegisterValid(APEX_CPU* cpu,int reg);




int isROBFull(struct ReorderBuffer* queue);
int isROBEmpty(struct ReorderBuffer* queue);
void pushToROB(struct ReorderBuffer* queue, CPU_Stage stage);
CPU_Stage* peekROBHead(struct ReorderBuffer* queue);
void deleteROBHead(struct ReorderBuffer* queue);
//int headOfROB(struct ReorderBuffer* queue);
//int tailOfROB(struct ReorderBuffer* queue);


int isLSQFull(struct LSQ* queue);
int isLSQEmpty(struct LSQ* queue);
void pushToLSQ(struct LSQ* queue, CPU_Stage stage);
CPU_Stage peekLSQHead(struct LSQ* queue);
void deleteLSQHead(struct LSQ* queue);
void addBackendRATEntry(APEX_CPU* cpu,int arch,int phy);

void checkForBackToBackExecution(APEX_CPU* cpu);

void printDataMemory(APEX_CPU* cpu);

CPU_Stage* peekROBTail(struct ReorderBuffer* queue);
void deleteROBTail(struct ReorderBuffer* queue);

CPU_Stage peekLSQTail(struct LSQ* queue);
void deleteLSQTail(struct LSQ* queue);

#endif
