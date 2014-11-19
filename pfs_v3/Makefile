
TARGETS := pfs

PFS_OBJS   := pfs.o

OBJS := $(PFS_OBJS)

CC := gcc

CFLAGS += -D_REENTRANT -D_LIBC_REENTRANT -D_THREAD_SAFE
CFLAGS += -Wall
CFLAGS += -Wunused
CFLAGS += -Wshadow
#CFLAGS += -Wdeclaration-after-statement
CFLAGS += -Wdisabled-optimization
CFLAGS += -Wpointer-arith
CFLAGS += -Wredundant-decls
CFLAGS += -g -O2
CFLAGS += -std=gnu99

# for FUSE
CFLAGS += -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=22
LDFLAGS += -lfuse

%.o: %.c
	$(CC) -o $*.o $< -c $(CFLAGS)

.PHONY: all clean mount umount

all: $(TARGETS)

clean:
	-rm -f $(TARGETS) $(OBJS) *~ *.bak core*

mount: pfs
	[ -d ./test ] || mkdir -p ./test
	./pfs ./test

umount:
	-[ -d ./test ] && fusermount -u ./test || exit 0
	rm -rf ./test

pfs: $(PFS_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

