# C compiler
# CC = gcc-mp-8
CC = cc
# CC = cc -m32 -mmacosx-version-min=10.6
STRIP = strip

# Threading
CFLAGS = -O2 -fomit-frame-pointer -DUSE_CURTERM
# CFLAGS = -O2 -fomit-frame-pointer -DSTC -DUSE_CURTERM
# CFLAGS = -O2 -fomit-frame-pointer -DSTC -DGCC_DISPATCH -DUSE_CURTERM

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
	$(STRIP) -x -S ceforth

clean:
	$(RM) eforth.new eforth.img
	$(RM) ceforth ceforth.boot
	
