#PFS_VERSION="0.1"

#EXTRA_CFLAGS += -DWRAPFS_VERSION=\"$(WRAPFS_VERSION)\"
obj-m := maindriver.o
maindriver-objs := main.o file.o lookup.o super.o mmap.o inode.o dentry.o

KDIR := /lib/modules/$(shell uname -r)/build

PWD := $(shell pwd)

all: main.c dentry.c file.c inode.c lookup.c super.c mmap.c 
	make -C $(KDIR) SUBDIRS=$(PWD) modules
clean:
	make -C $(KDIR) SUBDIRS=$(PWD) clean
		


