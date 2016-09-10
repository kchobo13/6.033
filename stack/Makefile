CC      := gcc
ASFLAGS := -m32
CFLAGS	:= -m32 -g -std=c99 -fno-stack-protector -Wall -D_GNU_SOURCE
LDFLAGS := -m32
PROGS	:= httpd-ex httpd-nx

all:	$(PROGS) shellcode.bin

%-ex: %.o
	$(CC) $(LDFLAGS) -z execstack $< -o $@

%-nx: %.o
	$(CC) $(LDFLAGS) $< -o $@

%.bin: %.o
	objcopy -S -O binary -j .text $< $@

clean:
	rm -f *.o *.pyc *.bin grades.txt $(PROGS)

.PRECIOUS: %.o
