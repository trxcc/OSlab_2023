#include "x86.h"
#include "device.h"

extern int displayRow;
extern int displayCol;

extern uint32_t keyBuffer[MAX_KEYBUFFER_SIZE];
extern int bufferHead;
extern int bufferTail;
extern int now_pointer;
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
	putChar('\n');
}

static void printStr(char *ch) {
	int i = 0;
	while (ch[i] != '\0') putChar(ch[i++]);
}

void GProtectFaultHandle(struct TrapFrame *tf);

void KeyboardHandle(struct TrapFrame *tf);

void syscallHandle(struct TrapFrame *tf);
void syscallWrite(struct TrapFrame *tf);
void syscallPrint(struct TrapFrame *tf);
void syscallRead(struct TrapFrame *tf);
void syscallGetChar(struct TrapFrame *tf);
void syscallGetStr(struct TrapFrame *tf);

int flag = 1;
void irqHandle(struct TrapFrame *tf) { // pointer tf = esp
	/*
	 * 中断处理程序
	 */
	/* Reassign segment register */
	asm volatile("movw %%ax, %%ds"::"a"(KSEL(SEG_KDATA)));
	//asm volatile("movw %%ax, %%es"::"a"(KSEL(SEG_KDATA)));
	//asm volatile("movw %%ax, %%fs"::"a"(KSEL(SEG_KDATA)));
	//asm volatile("movw %%ax, %%gs"::"a"(KSEL(SEG_KDATA)));
	//printInt(tf->irq);
	if (flag == 1) {
		printInt(100);
		printStr("fff");
		flag = 0;
	}
	switch(tf->irq) {
		// TODO: 填好中断处理程序的调用
		case -1: break;
		case 0xd: GProtectFaultHandle(tf); break;
		case 0x80: syscallHandle(tf); break;
		case 0x20: break;
		case 0x21: KeyboardHandle(tf); break; 
		default:assert(0);
	}
}

void GProtectFaultHandle(struct TrapFrame *tf){
	assert(0);
	return;
}

static void printStrToScreen(int head, int tail) {
	int size = tail - head;
	int sel = USEL(SEG_UDATA);
	char character = 0;
	uint16_t data = 0;
	int pos = 0;
	asm volatile("movw %0, %%es"::"m"(sel));
	for (int i = 0; i < size; i++) {
		asm volatile("movb %%es:(%1), %0":"=r"(character):"r"(keyBuffer+head+i));
		data = character | (0x0c << 8);
		pos = (80 * displayRow + displayCol) * 2;
		asm volatile("movw %0, (%1)"::"r"(data), "r"(pos + 0xb8000));
	}
}

void KeyboardHandle(struct TrapFrame *tf){
	//printStr("Into KeyboardHandle!\n");
	uint32_t code = getKeyCode();
	if(code == 0xe){ // 退格符
		// TODO: 要求只能退格用户键盘输入的字符串，且最多退到当行行首
		if (bufferTail > bufferHead && keyBuffer[bufferTail] != '\n') {
			int tmpHead = bufferTail - 1;
			while (tmpHead > bufferHead && keyBuffer[tmpHead - 1] != '\n') --tmpHead;
			displayCol = displayCol > 0 ? displayCol - 1 : displayCol;
			keyBuffer[bufferTail--] = '\0';
			printStrToScreen(tmpHead, bufferTail);
		}
	}else if(code == 0x1c){ // 回车符
		// TODO: 处理回车情况
		displayCol = 0;
		displayRow ++;
		keyBuffer[bufferTail++] = '\n';
	}else if(code < 0x81){ // 正常字符
		// TODO: 注意输入的大小写的实现、不可打印字符的处理
		char ch = getChar(code);
		if (ch >= 0x20) {
			putChar(ch);
			keyBuffer[bufferTail++] = ch;
			//printInt(bufferTail);
			/* *(char *)(tf->edx + tf->ebx) = ch;
			tf->ebx ++;
			syscallPrint(tf);*/
	//		printStrToScreen(bufferTail-1, bufferTail);
			int sel = USEL(SEG_UDATA);
			char character = ch;
			uint16_t data = 0;
			int pos = 0;
			asm volatile("movw %0, %%es"::"m"(sel));
			
			data = character | (0x0c << 8);
			pos = (80 * displayRow + displayCol) * 2;
			asm volatile("movw %0, (%1)"::"r"(data), "r"(pos + 0xb8000));
				
			displayCol ++;
			if (displayCol >= 80) {
				displayCol = 0;
				displayRow ++;
			}
			while (displayRow >= 25) {
				scrollScreen();
				displayRow --;
				displayCol = 0;
			}	
		}
	}
	updateCursor(displayRow, displayCol);
}

void syscallHandle(struct TrapFrame *tf) {
	switch(tf->eax) { // syscall number
		case 0:
			syscallWrite(tf);
			break; // for SYS_WRITE
		case 1:
			syscallRead(tf);
			break; // for SYS_READ
		default:break;
	}
}

void syscallWrite(struct TrapFrame *tf) {
	switch(tf->ecx) { // file descriptor
		case 0:
			syscallPrint(tf);
			break; // for STD_OUT
		default:break;
	}
}

void syscallPrint(struct TrapFrame *tf) {
	//printStr("Into syscallPrint\n");
	int sel = USEL(SEG_UDATA); //TODO: segment selector for user data, need further modification
	char *str = /*"hello\n";*/(char*)tf->edx;
	int size = /*6;*/tf->ebx;
	int i = 0;
	int pos = 0;
	char character = 0;
	uint16_t data = 0;
	asm volatile("movw %0, %%es"::"m"(sel));
	for (i = 0; i < size; i++) {
		asm volatile("movb %%es:(%1), %0":"=r"(character):"r"(str+i));
		if (character == '\n') {
			displayRow += 1;
			displayCol = 0;
		}
		else {
			data = character | (0x0c << 8);
			pos = (80 * displayRow + displayCol) * 2;
			asm volatile("movw %0, (%1)"::"r"(data), "r"(pos + 0xb8000));
			displayCol ++;
		}

		if (displayCol >= 80) {
			displayCol = 0;
			displayRow ++;
		}
		while (displayRow >= 25) {
			scrollScreen();
			displayRow --;
			displayCol = 0;
		} 
		// TODO: 完成光标的维护和打印到显存
	}
	
	updateCursor(displayRow, displayCol);
}

void syscallRead(struct TrapFrame *tf){
	switch(tf->ecx){ //file descriptor
		case 0:
			/*printStr("In sysRead\n");
			printStr("bufferHead:\n");
			printInt(bufferHead);
			printStr("bufferTail:\n");
			printInt(bufferTail);*/
			syscallGetChar(tf);
			break;
		case 1:
			syscallGetStr(tf);
			break; // for STD_STR
		default:break;
	}
}

void syscallGetChar(struct TrapFrame *tf){
	// TODO: 自由实现
	int flag = 0;
	if (keyBuffer[bufferTail - 1] == '\n') flag = 1;
	while (bufferTail > bufferHead && keyBuffer[bufferTail-1] == '\n') keyBuffer[--bufferTail] = '\0';
	if (bufferTail > bufferHead && flag == 1) tf->eax = keyBuffer[bufferHead++];
	else tf->eax = 0;
}

void syscallGetStr(struct TrapFrame *tf){
	// TODO: 自由实现
	int flag = 0;
	int i = 0;
	int sel = USEL(SEG_UDATA);
	asm volatile("movw %0, %%es"::"m"(sel));
#define min(a,b) (a < b ? a : b)
	//putChar(keyBuffer[bufferTail]);
	if (keyBuffer[bufferTail - 1] == '\n') flag = 1;
	while (bufferTail > bufferHead && keyBuffer[bufferTail-1] == '\n') keyBuffer[--bufferTail] = '\0';
	if (flag == 0 && bufferTail - bufferHead < tf->ebx) tf->eax = 0;
	else {
	//	printInt(bufferHead);
	//	printInt(bufferTail);
		printInt(bufferTail - bufferHead);
		for (i = 0; i < min(tf->ebx, bufferTail-bufferHead); i++) {
			asm volatile("movb %1, %%es:(%0)"::"r"(tf->edx+i), "r"(keyBuffer[bufferHead+i]));
		//	((char *)tf->edx)[i] = keyBuffer[bufferHead + i];
		}
		//printStr((char *)tf->edx);
		tf->eax = 1;
	}
}
