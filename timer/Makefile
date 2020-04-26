# Matthew Boyd
# EC535 Lab5
# Makefile for lab5timer (kernel module)

ifneq ($(KERNELRELEASE),)
	obj-m := lab5timer.o
else
	KERNELDIR := /ad/eng/courses/ec/ec535/bbb/stock/stock-linux-4.19.82-ti-rt-r33-fb
	PWD := $(shell pwd)
	ARCH := arm
	CROSS := arm-linux-gnueabihf-

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) ARCH=$(ARCH) clean

endif
