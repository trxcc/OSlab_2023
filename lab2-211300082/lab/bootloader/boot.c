#include "boot.h"
#include <string.h>
#define SECTSIZE 512

/*
void bootMain(void) {
	int i = 0;
	void (*elf)(void);
	elf = (void(*)(void))0x100000; // kernel is loaded to location 0x100000
	for (i = 0; i < 200; i ++) {
		//readSect((void*)elf + i*512, i+1);
		readSect((void*)elf + i*512, i+9);
	}
	elf(); // jumping to the loaded program
}
*/

void bootMain(void) {
	int i = 0;
	int phoff __attribute__((unused)) = 0x34;
	int offset = 0x1000;
	unsigned int elf = 0x100000;
	void (*kMainEntry)(void) __attribute__((unused));
	kMainEntry = (void(*)(void))0x100000;
	for (i = 0; i < 200; i++) {
		readSect((void*)(elf + i*512), 1+i);
	}

	// TODO: 填写kMainEntry、phoff、offset

	struct ELFHeader *ehdr = (void *)elf;
	kMainEntry = (void(*)(void))(ehdr->entry);
	phoff = ehdr->phoff;
	//offset = ((struct ProgramHeader *)(elf + phoff))->off;
//	struct ProgramHeader *phdr = (void *)(elf + phoff);
	//offset = phdr->off;

/*	unsigned short phnum = ehdr->phnum;

#define PT_LOAD 0x1
	for (unsigned short i = 0; i < phnum; i++) {
		struct ProgramHeader *now_phdr = phdr + i*ehdr->phentsize;
		if (now_phdr->type == PT_LOAD) {
			for (unsigned int j = 0; j < now_phdr->filesz; j++) {
				*(char *)(now_phdr->paddr + j) = *(char *)(ehdr->entry + now_phdr->off + j);
			}
		
			memcpy((void *)now_phdr->paddr, (void *)(elf + now_phdr->off), now_phdr->filesz);
			memset((void *)(now_phdr->paddr + now_phdr->filesz), 0, now_phdr->memsz - now_phdr->filesz);
		}
		
		for (unsigned int j = now_phdr->filesz; j < now_phdr->memsz; j++) {
			*(char *)(now_phdr->paddr + j) = 0;
		}
		
	}
*/
	for (i = 0; i < 200 * 512; i++) {
		*(unsigned char *)(elf + i) = *(unsigned char *)(elf + i + offset);
	}

	kMainEntry();
}

void waitDisk(void) { // waiting for disk
	while((inByte(0x1F7) & 0xC0) != 0x40);
}

void readSect(void *dst, int offset) { // reading a sector of disk
	int i;
	waitDisk();
	outByte(0x1F2, 1);
	outByte(0x1F3, offset);
	outByte(0x1F4, offset >> 8);
	outByte(0x1F5, offset >> 16);
	outByte(0x1F6, (offset >> 24) | 0xE0);
	outByte(0x1F7, 0x20);

	waitDisk();
	for (i = 0; i < SECTSIZE / 4; i ++) {
		((int *)dst)[i] = inLong(0x1F0);
	}
}
