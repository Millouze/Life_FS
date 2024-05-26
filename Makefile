obj-m += ouichefs.o 
ouichefs-objs := fs.o super.o inode.o file.o dir.o common.o read.o write.o ioctl.o defragmentation.o

KERNELDIR ?= /home/tara/linux-6.5.7
PWD := $(shell pwd)

all:
	make -C $(KERNELDIR) M=$(PWD) modules
	cp ouichefs.ko /home/tara/master/S2/PNL/share/

debug:
	make -C $(KERNELDIR) M=$(PWD) ccflags-y+="-DDEBUG -g" modules

# benchmark:
# 	gcc -Wall usercode/benchmark.c -o benchmark

clean:
	make -C $(KERNELDIR) M=$(PWD) clean
	rm -rf *~

.PHONY: all clean
