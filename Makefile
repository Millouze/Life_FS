obj-m += ouichefs.o 
ouichefs-objs := fs.o super.o inode.o file.o dir.o common.o read.o write.o ioctl.o defragmentation.o

KERNELDIR ?= /home/tara/linux-6.5.7
PWD := $(shell pwd)

all: benchmark benchmark_shift test_defrag unit_test
	make -C $(KERNELDIR) M=$(PWD) modules
	cp ouichefs.ko /home/tara/master/S2/PNL/share/

debug:
	make -C $(KERNELDIR) M=$(PWD) ccflags-y+="-DDEBUG -g" modules

benchmark:
	gcc -Wall usercode/benchmark.c usercode/utils.c -o benchmark

benchmark_shift:
	gcc -Wall usercode/benchmark_shift.c -o benchmark_shift

test_defrag:
	gcc -Wall usercode/test_defrag.c usercode/utils.c -o test_defrag

unit_test:
	gcc -Wall usercode/unit_test.c usercode/utils.c -o unit_test

clean:
	make -C $(KERNELDIR) M=$(PWD) clean
	rm -rf *~
	rm -rf benchmark benchmark_shift test_defrag unit_test

.PHONY: all clean
