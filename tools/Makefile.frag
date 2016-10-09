.PHONY: all clean

OBJS := mksfs.c
CFLAGS := -Wall -O2 -D_FILE_OFFSET_BITS=64

all: mksfs

mksfs: $(OBJS)
	$(CC) $(CFLAGS) -o $(OBJPATH_ROOT)/$@ $+
