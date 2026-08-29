# Makefile -- builds kernel.bin from boot.asm + kernel.c
# No cross-compiler needed: -m32 -ffreestanding on a normal gcc is enough
# for this simple a kernel.

CC = gcc
AS = nasm
LD = ld

CFLAGS = -m32 -ffreestanding -fno-stack-protector -fno-pie -O2 -Wall -Wextra
ASFLAGS = -f elf32
LDFLAGS = -m elf_i386 -T kernel/linker.ld -nostdlib

OBJS = kernel/boot.o kernel/kernel.o

kernel.bin: $(OBJS) kernel/linker.ld
	$(LD) $(LDFLAGS) -o kernel.bin $(OBJS)

kernel/boot.o: kernel/boot.asm
	$(AS) $(ASFLAGS) kernel/boot.asm -o kernel/boot.o

kernel/kernel.o: kernel/kernel.c
	$(CC) $(CFLAGS) -c kernel/kernel.c -o kernel/kernel.o

clean:
	rm -f kernel/*.o kernel.bin os.iso

.PHONY: clean
