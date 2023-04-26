#include "device.h"
#include "x86.h"

extern int displayRow;
extern int displayCol;
extern TSS tss;
extern ProcessTable pcb[];
extern int current;

void GProtectFaultHandle(struct StackFrame *sf);
void timerHandle(struct StackFrame *sf);
void syscallHandle(struct StackFrame *sf);

void syscallWrite(struct StackFrame *sf);
void syscallPrint(struct StackFrame *sf);
void syscallFork(struct StackFrame *sf);
void syscallSleep(struct StackFrame *sf);
void syscallExit(struct StackFrame *sf);

static void printStr(char *str) {
	while(*str != '\0') putChar(*str++);
}

#define MAX_SIZE 256

static int dec2Str(int decimal, char *buffer, int size, int count) {
	int i=0;
	int temp;
	int number[16];

	if(decimal<0){
		buffer[count]='-';
		count++;
		if(count==size) {
		//	syscall(SYS_WRITE, STD_OUT, (uint32_t)buffer, (uint32_t)size, 0, 0);
			count=0;
		}
		temp=decimal/10;
		number[i]=temp*10-decimal;
		decimal=temp;
		i++;
		while(decimal!=0){
			temp=decimal/10;
			number[i]=temp*10-decimal;
			decimal=temp;
			i++;
		}
	}
	else{
		temp=decimal/10;
		number[i]=decimal-temp*10;
		decimal=temp;
		i++;
		while(decimal!=0){
			temp=decimal/10;
			number[i]=decimal-temp*10;
			decimal=temp;
			i++;
		}
	}

	while(i!=0){
		buffer[count]=number[i-1]+'0';
		count++;
		if(count==size) {
		//	syscall(SYS_WRITE, STD_OUT, (uint32_t)buffer, (uint32_t)size, 0, 0);
			count=0;
		}
		i--;
	}
	return count;
}

static void printInt(int n) {
   	char str[MAX_SIZE];
	int count = 0, i = 0;
	count = dec2Str(n, str, MAX_SIZE, count);
	while (str[i] != '\0') putChar(str[i++]);
//	putChar('\n');
}

void irqHandle(struct StackFrame *sf) {  // pointer sf = esp
    /* Reassign segment register */
    asm volatile("movw %%ax, %%ds" ::"a"(KSEL(SEG_KDATA)));
    /* Save esp to stackTop */
    uint32_t tmpStackTop = pcb[current].stackTop;
    pcb[current].prevStackTop = pcb[current].stackTop;
    pcb[current].stackTop = (uint32_t)sf;
//	printInt(sf->irq);
	int i = 0;
	if (i) printInt(i);
    switch (sf->irq) {
        case -1:
            break;
        case 0xd:
            GProtectFaultHandle(sf);
            break;
        case 0x20:
            timerHandle(sf);
            break;
        case 0x80:
            syscallHandle(sf);
            break;
        default:
            assert(0);
    }

    /* Recover stackTop */
    pcb[current].stackTop = tmpStackTop;
}

void GProtectFaultHandle(struct StackFrame *sf) {
    assert(0);
    return;
}

void syscallHandle(struct StackFrame *sf) {
//	printStr("sf->eax: ");
//	printInt(sf->eax);
	switch (sf->eax) {  // syscall number
        case 0:
            syscallWrite(sf);
            break;  // for SYS_WRITE (0)
        /*TODO Add Fork,Sleep... */
		case 1:
			syscallFork(sf);
			break;
		/*case 2:
			syscallExec(sf);
			break;*/
		case 3:
			syscallSleep(sf);
			break;
		case 4:
			syscallExit(sf);
			break;
		default:
            break;
    }
}

#define switch_to_kernel() do {\
	uint32_t tmpStackTop = pcb[current].stackTop;\
	pcb[current].stackTop = pcb[current].prevStackTop;\
	tss.esp0 = (uint32_t)&(pcb[current].stackTop);\
	asm volatile("movl %0, %%esp"::"m"(tmpStackTop));\
	asm volatile("popl %gs");\
	asm volatile("popl %fs");\
	asm volatile("popl %es");\
	asm volatile("popl %ds");\
	asm volatile("popal");\
	asm volatile("addl $8, %esp");\
	asm volatile("iret");} while(0)

void timerHandle(struct StackFrame *sf) {
    // TODO in lab3
	for (int i = 0; i < MAX_PCB_NUM; i++) {
		if (pcb[i].state == STATE_BLOCKED) {
			if (pcb[i].sleepTime > 0) pcb[i].sleepTime--;
			if (pcb[i].sleepTime == 0) pcb[i].state = STATE_RUNNABLE;
		}
	}	
	if (pcb[current].state == STATE_RUNNING && pcb[current].timeCount < MAX_TIME_COUNT) {
		pcb[current].timeCount++;
		return;
	}
	pcb[current].timeCount = 0;
	pcb[current].state = STATE_RUNNABLE;
	
	for (int i = (current + 1) % MAX_PCB_NUM; i != current; i = (i+1) % MAX_PCB_NUM) {
		if (pcb[i].state == STATE_RUNNABLE && i != 0) {
			current = i;
			break;
		}
	}
	pcb[current].state = STATE_RUNNING;
	switch_to_kernel();	
}

void syscallWrite(struct StackFrame *sf) {
    switch (sf->ecx) {  // file descriptor
        case 0:
            syscallPrint(sf);
            break;  // for STD_OUT
        default:
            break;
    }
}

// Attention:
// This is optional homework, because now our kernel can not deal with
// consistency problem in syscallPrint. If you want to handle it, complete this
// function. But if you're not interested in it, don't change anything about it
void syscallPrint(struct StackFrame *sf) {
    int sel = sf->ds;  // TODO segment selector for user data, need further
                       // modification
    char *str = (char *)sf->edx;
    int size = sf->ebx;
    int i = 0;
    int pos = 0;
    char character = 0;
    uint16_t data = 0;
    asm volatile("movw %0, %%es" ::"m"(sel));
    for (i = 0; i < size; i++) {
        asm volatile("movb %%es:(%1), %0" : "=r"(character) : "r"(str + i));
        if (character == '\n') {
            displayRow++;
            displayCol = 0;
            if (displayRow == 25) {
                displayRow = 24;
                displayCol = 0;
                scrollScreen();
            }
        } else {
            data = character | (0x0c << 8);
            pos = (80 * displayRow + displayCol) * 2;
            asm volatile("movw %0, (%1)" ::"r"(data), "r"(pos + 0xb8000));
            displayCol++;
            if (displayCol == 80) {
                displayRow++;
                displayCol = 0;
                if (displayRow == 25) {
                    displayRow = 24;
                    displayCol = 0;
                    scrollScreen();
                }
            }
        }
         //asm volatile("int $0x20"); //XXX Testing irqTimer during syscall
        // asm volatile("int $0x20":::"memory"); //XXX Testing irqTimer during
        // syscall
    }

    updateCursor(displayRow, displayCol);
    // TODO take care of return value
    return;
}

void syscallFork(struct StackFrame *sf) {
    // TODO in lab3
	printStr("Into syscallFork\n");
	int new_index = 0;
	for (new_index = 0; new_index < MAX_PCB_NUM && pcb[new_index].state != STATE_DEAD; new_index++);
	if (new_index == MAX_PCB_NUM) {
		pcb[current].regs.eax = -1;
		return;
	}
	enableInterrupt();
	for (int i = 0; i < 0x100000; i++) {
		*(unsigned char *)(i + (new_index + 1) * 0x100000) = *(unsigned char *)(i + (current + 1) * 0x100000);
		asm volatile("int $0x20");
	}
	disableInterrupt();

	pcb[new_index].pid = new_index;
	pcb[new_index].prevStackTop = pcb[current].prevStackTop - (uint32_t)&(pcb[current]) + (uint32_t)&(pcb[new_index]);
	pcb[new_index].sleepTime = 0;

	pcb[new_index].stackTop = pcb[current].stackTop - (uint32_t)&(pcb[current]) + (uint32_t)&(pcb[new_index]);

	pcb[new_index].state = STATE_RUNNABLE;
	pcb[new_index].timeCount = 0;

	pcb[new_index].regs.edi = pcb[current].regs.edi;
	pcb[new_index].regs.esi = pcb[current].regs.esi;
	pcb[new_index].regs.ebp = pcb[current].regs.ebp;
	pcb[new_index].regs.xxx = pcb[current].regs.xxx;
	pcb[new_index].regs.ebx = pcb[current].regs.ebx;
	pcb[new_index].regs.edx = pcb[current].regs.edx;
	pcb[new_index].regs.ecx = pcb[current].regs.ecx;
	pcb[new_index].regs.eax = pcb[current].regs.eax;
	pcb[new_index].regs.irq = pcb[current].regs.irq;
	pcb[new_index].regs.error = pcb[current].regs.error;
	pcb[new_index].regs.eip = pcb[current].regs.eip;
	pcb[new_index].regs.eflags = pcb[current].regs.eflags;
	pcb[new_index].regs.esp = pcb[current].regs.esp;

	pcb[new_index].regs.cs = USEL(1 + 2*new_index);
	pcb[new_index].regs.ss = USEL(2 * (new_index+1));
	pcb[new_index].regs.ds = USEL(2 * (new_index+1));
	pcb[new_index].regs.es = USEL(2 * (new_index+1));
	pcb[new_index].regs.fs = USEL(2 * (new_index+1));
	pcb[new_index].regs.gs = USEL(2 * (new_index+1));

//	pcb[current].state = STATE_RUNNABLE;
	pcb[current].regs.eax = new_index;
	pcb[new_index].regs.eax = 0;
}

#define find_new_running_pcb() do {\
	int find_runnable_pcb = 0; \
	for (int i = (current + 1) % MAX_PCB_NUM; i != current; i = (i+1) % MAX_PCB_NUM) {\
		if (pcb[i].state == STATE_RUNNABLE && i != 0) {\
			find_runnable_pcb = 1;\
			current = i;\
			break;\
		}\
	}\
	if (find_runnable_pcb == 0) current = 0;\
	pcb[current].state = STATE_RUNNING;\
 } while(0)

void syscallSleep(struct StackFrame *sf) {
    // TODO in lab3
	printStr("Into syscallSleep\n");
	pcb[current].state = STATE_BLOCKED;

	find_new_running_pcb();

//	printStr("hit here");

	pcb[current].state = STATE_RUNNING;

	switch_to_kernel();
}


void syscallExit(struct StackFrame *sf) {
    // TODO in lab3
	printStr("Into syscallExit\n");
	pcb[current].state = STATE_DEAD;

	find_new_running_pcb();

	switch_to_kernel();
}
