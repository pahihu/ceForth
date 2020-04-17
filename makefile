# C compiler
# CC = gcc-mp-8
CC = cc
# CC = cc -m32 -mmacosx-version-min=10.6

# Threading
CFLAGS = -O2 -fomit-frame-pointer
# CFLAGS = -O2 -fomit-frame-pointer -DSTC
# CFLAGS = -O2 -fomit-frame-pointer -DSTC -DGCC_DISPATCH

SRC = curterm.c ceforth.c
OBJ = curterm.o ceforth.o

all: eforth.img

eforth.img: eforth.new ceforth
	cp eforth.new eforth.img

eforth.new: ceforth.boot
	./ceforth.boot

ceforth.boot: $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC) -DBOOT

ceforth: $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC)

clean:
	$(RM) eforth.new eforth.img
	$(RM) ceforth ceforth.boot
	
