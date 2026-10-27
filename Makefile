KDIR ?= $(HOME)/linux
ARCH ?= arm
CROSS_COMPILE ?= arm-linux-gnueabihf-

obj-m += hello_mod/
obj-m += demo_drv/

all:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f dts/*.dtbo

.PHONY: all clean
