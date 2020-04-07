/******************************************************************************/
/* ceForth_33.cpp, Version 3.3 : Forth in C                                   */
/******************************************************************************/

/* Chen-Hanson Ting                                                           */
/* 01jul19cht   version 3.3                                                   */
/* Macro assembler, Visual Studio 2019 Community                              */
/* 13jul17cht   version 2.3                                                   */
/* True byte code machine with bytecode                                       */
/* Change w to WP, pointing to parameter field                                */
/* 08jul17cht  version 2.2                                                    */
/* Stacks are 256 cell circular buffers                                       */
/* Clean up, delete SP@, SP!, RP@, RP!                                        */
/* 13jun17cht  version 2.1                                                    */
/* Compiled as a C++ console project in Visual Studio Community 2017          */
/* Follow the eForth model with 64 primitives                                 */
/* Kernel                                                                     */
/* Use int64_t to implement multipy and divide primitives                     */
/* Case insensitive interpreter                                               */
/* data[] must be filled with rom_21.h eForth dictionary                      */
/*   from c:/F#/ceforth_21                                                    */
/* C compiler must be reminded that S and R are (char)                        */
/******************************************************************************/

//Preamble

#include <stdlib.h>
#include <stdio.h>
#ifndef __APPLE__
#include <tchar.h>
#endif
#include <stdarg.h>
#include <string.h>

#include <stdint.h>

#include <unistd.h>
#ifdef USE_CURTERM
#include "curterm.h"
#endif

#define BytesPerWord 8

# if BytesPerWord==8

typedef int64_t word_t;
typedef uint64_t uword_t;
typedef int64_t  dword_t;
typedef uint64_t udword_t;
# define TOWORDS(x) ((x) >> 3)
# define TOBYTES(x) ((x) << 3)
# define UpperMask  ((uword_t)0x5F5F5F5F5F5F5F5FULL)
# define VaType     word_t

# elif BytesPerWord==4

typedef int32_t word_t;
typedef uint32_t uword_t;
typedef int64_t  dword_t;
typedef uint64_t udword_t;
# define TOWORDS(x) ((x) >> 2)
# define TOBYTES(x) ((x) << 2)
# define UpperMask  ((uword_t)0x5F5F5F5FUL)
# define VaType     word_t

# elif BytesPerWord==2

typedef int16_t word_t;
typedef uint16_t uword_t;
typedef int32_t  dword_t;
typedef uint32_t udword_t;
# define TOWORDS(x) ((x) >> 1)
# define TOBYTES(x) ((x) << 1)
# define UpperMask  ((uword_t)0x5F5FU)
# define VaType     int

# else
# error Unknown word size!
# endif

# define        BitsPerWord     (8*(BytesPerWord))
# define        Round(x)        ((((x) + BytesPerWord - 1) / BytesPerWord) * BytesPerWord)

# define	FALSE	0
# define	TRUE	-1
# define	LOGICAL ? TRUE : FALSE
# define 	LOWER(x,y) ((uword_t)(x)<(uword_t)(y))
# define	pop	top = stack[255 & (S--)]
# define	push	stack[255 & (++S)] = top; top =
# define	popR    rack[255 & (R--)]
# define	pushR   rack[255 & (++R)]

# define        CODEALIGN   while (P & (BytesPerWord-1)) { cData[P++] = 0; }
# define        SYSVARBASE  0X80


/*
#define STC             1
#define GCC_DISPATCH    1
*/


word_t  IZ, thread;

#ifdef STC
word_t rack[256] = { 0 };
int R = 0;
word_t  P, IP;
#endif

word_t data[16000] = {};
unsigned char* cData = (unsigned char*)data;

#define do_list(x) \
        x(nop)   x(bye)    x(qrx)   x(txsto) x(docon) x(dolit) x(dolist) x(exitt) \
        x(execu) x(donext) x(qbran) x(bran)  x(store) x(at)    x(cstor)  x(cat)   \
        x(rpat)  x(rpsto)  x(rfrom) x(rat)   x(tor)   x(spat)  x(spsto)  x(drop)  \
        x(dup)   x(swap)   x(over)  x(zless) x(andd)  x(orr)   x(xorr)   x(uplus) \
        x(next)  x(qdup)   x(rot)   x(ddrop) x(ddup)  x(plus)  x(inver)  x(negat) \
        x(dnega) x(subb)   x(abss)  x(equal) x(uless) x(less)  x(ummod)  x(msmod) \
        x(slmod) x(mod)    x(slash) x(umsta) x(star)  x(mstar) x(ssmod)  x(stasl) \
        x(pick)  x(pstor)  x(dstor) x(dat)   x(count) x(dovar) x(max)    x(min)   \
        x(clit)  x(sys)

// Byte Code Assembler

#define do_as(x)        as_##x,
enum { do_list(do_as)   as_numbytecodes } ByteCodes;


#ifdef STC
static void stc(void) {
#else
#define do_decl(x)      static void P##x(void);
do_list(do_decl)
#endif

word_t rack[256] = { 0 };
word_t stack[256] = { 0 };
dword_t d, n, m;
int R = 0;
int S = 0;
word_t  top = 0;
word_t  P, IP, WP;
unsigned char bytecode, c;
#ifdef GCC_DISPATCH
#define do_label(x)     &&L##x,
        static void *dispatch[] = {
                do_list(do_label)
                0
        };
#endif


#define macNEXT         P = data[TOWORDS(IP)]; WP = P + BytesPerWord; IP += BytesPerWord;
#define macDROP         pop
#define macOVER 	push stack[255 & (S - 1)];
#define macINVER	top = -top - 1;
#define macTOR          rack[255 & (++R)] = top; pop;
#define macUPLUS        stack[255 & S] += top; top = LOWER(stack[255 & S], top);
#define macRFROM        push rack[255 & (R--)];
#define macPLUS         top += stack[255 & (S--)];

#ifdef STC 
# define FETCH_CODE      bytecode = (unsigned char)cData[P++];
# define doNEXT          macNEXT
# ifdef GCC_DISPATCH
#  define PRIMITIVE(x)   L##x:
#  ifdef __clang__
#   define NEXT_BYTECODE
#   define PEND          FETCH_CODE; goto *dispatch[bytecode];
#  else
#   define NEXT_BYTECODE FETCH_CODE
#   define PEND          goto *dispatch[bytecode];
#  endif
# else
#  define PRIMITIVE(x)   case as_##x:
#  define PEND           FETCH_CODE; break;
# endif
#else
# define PRIMITIVE(x)    static void P##x(void) {
# define PEND            }
# define doNEXT          Pnext()
# define NEXT_BYTECODE
#endif

// Virtual Forth Machine
#ifdef STC
	P = 0;
	WP = BytesPerWord;
	IP = 0;
	S = 0;
	R = 0;
	top = 0;
#ifdef GCC_DISPATCH
        PEND;
#else
        FETCH_CODE;
        while (TRUE) {
                switch (bytecode) {
#endif
#endif

PRIMITIVE(bye)
#ifdef _CURTERM_H
        prepterm(0);
#endif
	exit(0);
PEND
PRIMITIVE(next)
        macNEXT;
        NEXT_BYTECODE;
PEND
PRIMITIVE(qrx)
	doNEXT;
        NEXT_BYTECODE;
PEND
PRIMITIVE(txsto)
	doNEXT;
        NEXT_BYTECODE;
PEND
PRIMITIVE(dovar)
        NEXT_BYTECODE;
	push WP;
PEND
PRIMITIVE(docon)
        NEXT_BYTECODE;
	push data[TOWORDS(WP)];
PEND
PRIMITIVE(dolit)
	push data[TOWORDS(IP)];
	IP += BytesPerWord;
	doNEXT;
        NEXT_BYTECODE;
PEND
PRIMITIVE(dolist)
	rack[255 & (++R)] = IP;
	IP = WP;
	doNEXT;
        NEXT_BYTECODE;
PEND
PRIMITIVE(exitt)
	IP = (word_t)rack[255 & (R--)];
	doNEXT;
        NEXT_BYTECODE;
PEND
PRIMITIVE(execu)
	P = top;
	WP = P + BytesPerWord;
	pop;
        NEXT_BYTECODE;
PEND
PRIMITIVE(donext)
	if (rack[255 & R]) {
		rack[255 & R] -= 1;
		IP = data[TOWORDS(IP)];
	}
	else {
		IP += BytesPerWord;
		R--;
	}
	doNEXT;
        NEXT_BYTECODE;
PEND
PRIMITIVE(qbran)
	if (top == 0) IP = data[TOWORDS(IP)];
	else IP += BytesPerWord;
	pop;
	doNEXT;
        NEXT_BYTECODE;
PEND
PRIMITIVE(bran)
	IP = data[TOWORDS(IP)];
	doNEXT;
        NEXT_BYTECODE;
PEND
PRIMITIVE(store)
        NEXT_BYTECODE;
	data[TOWORDS(top)] = stack[255 & (S--)];
	pop;
PEND
PRIMITIVE(at)
        NEXT_BYTECODE;
	top = data[TOWORDS(top)];
PEND
PRIMITIVE(cstor)
        NEXT_BYTECODE;
	cData[top] = (char)stack[255 & (S--)];
	pop;
PEND
PRIMITIVE(cat)
        NEXT_BYTECODE;
	top = (word_t)cData[top];
PEND
PRIMITIVE(rpat)
        doNEXT;
        NEXT_BYTECODE;
PEND
PRIMITIVE(rpsto)
        doNEXT;
        NEXT_BYTECODE;
PEND
PRIMITIVE(rfrom)
        NEXT_BYTECODE;
        macRFROM;
PEND
PRIMITIVE(rat)
        NEXT_BYTECODE;
	push rack[255 & R];
PEND
PRIMITIVE(tor)
        NEXT_BYTECODE;
        macTOR;
PEND
PRIMITIVE(spat)
        doNEXT;
        NEXT_BYTECODE;
PEND
PRIMITIVE(spsto)
        doNEXT;
        NEXT_BYTECODE;
PEND
PRIMITIVE(drop)
        NEXT_BYTECODE;
        macDROP;
PEND
PRIMITIVE(dup)
        NEXT_BYTECODE;
	stack[255 & (++S)] = top;
PEND
PRIMITIVE(swap)
        NEXT_BYTECODE;
	WP = top;
	top = stack[255 & S];
	stack[255 & S] = WP;
PEND
PRIMITIVE(over)
        NEXT_BYTECODE;
        macOVER;
PEND
PRIMITIVE(zless)
        NEXT_BYTECODE;
	top = (top < 0) LOGICAL;
PEND
PRIMITIVE(andd)
        NEXT_BYTECODE;
	top &= stack[255 & (S--)];
PEND
PRIMITIVE(orr)
        NEXT_BYTECODE;
	top |= stack[255 & (S--)];
PEND
PRIMITIVE(xorr)
        NEXT_BYTECODE;
	top ^= stack[255 & (S--)];
PEND
PRIMITIVE(uplus)
        NEXT_BYTECODE;
        macUPLUS;
PEND
PRIMITIVE(nop)
	doNEXT;
        NEXT_BYTECODE;
PEND
PRIMITIVE(qdup)
        NEXT_BYTECODE;
	if (top) stack[255 & (++S)] = top;
PEND
PRIMITIVE(rot)
        NEXT_BYTECODE;
	WP = stack[255 & (S - 1)];
	stack[255 & (S - 1)] = stack[255 & S];
	stack[255 & S] = top;
	top = WP;
PEND
PRIMITIVE(ddrop)
        NEXT_BYTECODE;
	macDROP; macDROP;
PEND
PRIMITIVE(ddup)
        NEXT_BYTECODE;
        macOVER; macOVER;
PEND
PRIMITIVE(plus)
        NEXT_BYTECODE;
        macPLUS;
PEND
PRIMITIVE(inver)
        NEXT_BYTECODE;
        macINVER;
PEND
PRIMITIVE(negat)
        NEXT_BYTECODE;
	top = 0 - top;
PEND
PRIMITIVE(dnega)
        NEXT_BYTECODE;
        macINVER;
        macTOR;
        macINVER;
	push 1;
        macUPLUS;
	macRFROM;
	macPLUS;
PEND
PRIMITIVE(subb)
        NEXT_BYTECODE;
	top = stack[255 & (S--)] - top;
PEND
PRIMITIVE(abss)
        NEXT_BYTECODE;
	if (top < 0)
		top = -top;
PEND
/*
PRIMITIVE(great)
	top = (stack[255 & (S--)] > top) LOGICAL;
PEND
*/
PRIMITIVE(less)
        NEXT_BYTECODE;
	top = (stack[255 & (S--)] < top) LOGICAL;
PEND
PRIMITIVE(equal)
        NEXT_BYTECODE;
	top = (stack[255 & (S--)] == top) LOGICAL;
PEND
PRIMITIVE(uless)
        NEXT_BYTECODE;
	top = LOWER(stack[255 & S], top) LOGICAL; S--;
PEND
PRIMITIVE(ummod)
        NEXT_BYTECODE;
	d = (dword_t)((uword_t)top);
	m = (dword_t)((uword_t)stack[255 & S]);
	n = (dword_t)((uword_t)stack[255 & (S - 1)]);
# if BytesPerWord != 8
	n += m << BitsPerWord;
#endif
	pop;
	top = (uword_t)(n / d);
	stack[255 & S] = (uword_t)(n % d);
PEND
PRIMITIVE(msmod)
        NEXT_BYTECODE;
	d = (dword_t)((word_t)top);
	m = (dword_t)((word_t)stack[255 & S]);
	n = (dword_t)((word_t)stack[255 & (S - 1)]);
# if BytesPerWord != 8
	n += m << BitsPerWord;
# endif
	pop;
	top = (word_t)(n / d);
	stack[255 & S] = (word_t)(n % d);
PEND
PRIMITIVE(slmod)
        NEXT_BYTECODE;
	if (top != 0) {
		WP = stack[255 & S] / top;
		stack[255 & S] %= top;
		top = WP;
	}
PEND
PRIMITIVE(mod)
        NEXT_BYTECODE;
	top = (top) ? stack[255 & (S--)] % top : stack[255 & (S--)];
PEND
PRIMITIVE(slash)
        NEXT_BYTECODE;
	top = (top) ? stack[255 & (S--)] / top : (stack[255 & (S--)], 0);
PEND
PRIMITIVE(umsta)
        NEXT_BYTECODE;
	d = (udword_t)top;
	m = (udword_t)stack[255 & S];
	m *= d;
# if BytesPerWord == 8
        top = 0;
# else
	top = (uword_t)(m >> BitsPerWord);
# endif
	stack[255 & S] = (uword_t)m;
PEND
PRIMITIVE(star)
        NEXT_BYTECODE;
	top *= stack[255 & (S--)];
PEND
PRIMITIVE(mstar)
        NEXT_BYTECODE;
	d = (dword_t)top;
	m = (dword_t)stack[255 & S];
	m *= d;
# if BytesPerWord == 8
        top = 0;
# else
	top = (word_t)(m >> BitsPerWord);
# endif
	stack[255 & (S)] = (word_t)m;
PEND
PRIMITIVE(ssmod)
        NEXT_BYTECODE;
	d = (dword_t)top;
	m = (dword_t)stack[255 & S];
	n = (dword_t)stack[255 & (S - 1)];
	n *= m;
	pop;
	top = (word_t)(n / d);
	stack[255 & S] = (word_t)(n % d);
PEND
PRIMITIVE(stasl)
        NEXT_BYTECODE;
	d = (dword_t)top;
	m = (dword_t)stack[255 & S];
	n = (dword_t)stack[255 & (S - 1)];
	n *= m;
	pop; pop;
	top = (word_t)(n / d);
PEND
PRIMITIVE(pick)
        NEXT_BYTECODE;
	top = stack[255 & (S - top)];
PEND
PRIMITIVE(pstor)
        NEXT_BYTECODE;
	data[TOWORDS(top)] += stack[255 & (S--)], pop;
PEND
PRIMITIVE(dstor)
        NEXT_BYTECODE;
	data[TOWORDS(top) + 1] = stack[255 & (S--)];
	data[TOWORDS(top)] = stack[255 & (S--)];
	pop;
PEND
PRIMITIVE(dat)
        NEXT_BYTECODE;
	push data[TOWORDS(top)];
	top = data[TOWORDS(top) + 1];
PEND
PRIMITIVE(count)
        NEXT_BYTECODE;
	stack[255 & (++S)] = top + 1;
	top = cData[top];
PEND
PRIMITIVE(max)
        NEXT_BYTECODE;
	if (top < stack[255 & S]) pop;
	else S--;
PEND
PRIMITIVE(min)
        NEXT_BYTECODE;
	if (top < stack[255 & S]) S--;
	else pop;
PEND
PRIMITIVE(clit)
        push (unsigned char)cData[P++];
        NEXT_BYTECODE;
PEND
PRIMITIVE(sys)
        NEXT_BYTECODE;
        WP = top; pop;
        switch (WP) {
        case 0: /* ?RX */
#ifdef _CURTERM_H
                if (isatty(fileno(stdin))) {
                        if (has_key()) {
                                push(word_t) getkey();
                                kbflush();
                        } else {
                                push 0;
                        }
                } else {
                        push(word_t) getchar();
                }
#else
	        push(word_t) getchar();
#endif
	        if (top != 0) {
                        // printf ("?RX => %d\n", top);
                        push TRUE;
                }
                break;
        case 1: /* TX! */
#ifdef _CURTERM_H
                prepterm(0);
                putchar((char)top);
                prepterm(1);
                if (isatty(fileno(stdout)))
                        fflush(stdout);
#else
	        putchar((char)top);
#endif
	        pop;
                break;
        case 2: /* MS */
                usleep(1000 * top);
                pop;
                break;
        default:
                printf("unknown SYS %X\n", WP);
        }
PEND

#ifdef STC
#ifdef GCC_DISPATCH
        }
#else
        }}}
#endif
#endif

#ifndef STC
void(*primitives[as_numbytecodes])(void) = {
	/* case 0 */ Pnop,
	/* case 1 */ Pbye,
	/* case 2 qrx */ Pnop,
	/* case 3 txsto */ Pnop,
	/* case 4 */ Pdocon,
	/* case 5 */ Pdolit,
	/* case 6 */ Pdolist,
	/* case 7 */ Pexitt,
	/* case 8 */ Pexecu,
	/* case 9 */ Pdonext,
	/* case 10 */ Pqbran,
	/* case 11 */ Pbran,
	/* case 12 */ Pstore,
	/* case 13 */ Pat,
	/* case 14 */ Pcstor,
	/* case 15 */ Pcat,
	/* case 16  rpat, */ Pnop,
	/* case 17  rpsto, */ Pnop,
	/* case 18 */ Prfrom,
	/* case 19 */ Prat,
	/* case 20 */ Ptor,
	/* case 21 spat, */ Pnop,
	/* case 22 spsto, */ Pnop,
	/* case 23 */ Pdrop,
	/* case 24 */ Pdup,
	/* case 25 */ Pswap,
	/* case 26 */ Pover,
	/* case 27 */ Pzless,
	/* case 28 */ Pandd,
	/* case 29 */ Porr,
	/* case 30 */ Pxorr,
	/* case 31 */ Puplus,
	/* case 32 */ Pnext,
	/* case 33 */ Pqdup,
	/* case 34 */ Prot,
	/* case 35 */ Pddrop,
	/* case 36 */ Pddup,
	/* case 37 */ Pplus,
	/* case 38 */ Pinver,
	/* case 39 */ Pnegat,
	/* case 40 */ Pdnega,
	/* case 41 */ Psubb,
	/* case 42 */ Pabss,
	/* case 43 */ Pequal,
	/* case 44 */ Puless,
	/* case 45 */ Pless,
	/* case 46 */ Pummod,
	/* case 47 */ Pmsmod,
	/* case 48 */ Pslmod,
	/* case 49 */ Pmod,
	/* case 50 */ Pslash,
	/* case 51 */ Pumsta,
	/* case 52 */ Pstar,
	/* case 53 */ Pmstar,
	/* case 54 */ Pssmod,
	/* case 55 */ Pstasl,
	/* case 56 */ Ppick,
	/* case 57 */ Ppstor,
	/* case 58 */ Pdstor,
	/* case 59 */ Pdat,
	/* case 60 */ Pcount,
	/* case 61 */ Pdovar,
	/* case 62 */ Pmax,
	/* case 63 */ Pmin,
        /* case 64 */ Pclit,
        /* case 65 */ Psys
};
#endif

#ifdef BOOT
// Macro Assembler

const word_t IMEDD = 0x80;
const word_t COMPO = 0x40;
word_t BRAN = 0, QBRAN = 0, DONXT = 0, DOTQP = 0, STRQP = 0, TOR = 0, ABORQP = 0;

void HEADER(int lex, const char seq[]) {
	IP = TOWORDS(P);
	int i;
	int len = lex & 31;
	data[IP++] = thread;
	P = TOBYTES(IP);
	//printf("\n%X",thread);
	//for (i = thread >> 2; i < IP; i++)
	//{	printf(" %X",data[i]); }
	thread = P;
	cData[P++] = lex;
	for (i = 0; i < len; i++)
	{
		cData[P++] = seq[i];
	}
        CODEALIGN;
	printf("\n");
	printf("%s",seq);
	printf(" %X", P);
}
word_t CODE(int len, ...) {
	word_t addr = P;
	va_list argList;
	va_start(argList, len);
	for (; len; len--) {
		word_t j = va_arg(argList, VaType);
		cData[P++] = j;
		//printf(" %X",j);
	}
        CODEALIGN;
	va_end(argList);
	return addr;
}
word_t COLON(int len, ...) {
	word_t addr = P;
	IP = TOWORDS(P);
	data[IP++] = as_dolist; // dolist
	va_list argList;
	va_start(argList, len);
	//printf(" %X ",6);
	for (; len; len--) {
		word_t j = va_arg(argList, VaType);
		data[IP++] = j;
		//printf(" %X",j);
	}
	P = TOBYTES(IP);
	va_end(argList);
	return addr;
}
word_t LABEL(int len, ...) {
	word_t addr = P;
	IP = TOWORDS(P);
	va_list argList;
	va_start(argList, len);
	//printf("\n%X ",addr);
	for (; len; len--) {
		word_t j = va_arg(argList, VaType);
		data[IP++] = j;
		//printf(" %X",j);
	}
	P = TOBYTES(IP);
	va_end(argList);
	return addr;
}
void BEGIN(int len, ...) {
	IP = TOWORDS(P);
	//printf("\n%X BEGIN ",P);
	pushR = IP;
	va_list argList;
	va_start(argList, len);
	for (; len; len--) {
		word_t j = va_arg(argList, VaType);
		data[IP++] = j;
		//printf(" %X",j);
	}
	P = TOBYTES(IP);
	va_end(argList);
}
void AGAIN(int len, ...) {
	IP = TOWORDS(P);
	//printf("\n%X AGAIN ",P);
	data[IP++] = BRAN;
	data[IP++] = TOBYTES(popR);
	va_list argList;
	va_start(argList, len);
	for (; len; len--) {
		word_t j = va_arg(argList, VaType);
		data[IP++] = j;
		//printf(" %X",j);
	}
	P = TOBYTES(IP);
	va_end(argList);
}
void UNTIL(int len, ...) {
	IP = TOWORDS(P);
	//printf("\n%X UNTIL ",P);
	data[IP++] = QBRAN;
	data[IP++] = TOBYTES(popR);
	va_list argList;
	va_start(argList, len);
	for (; len; len--) {
		word_t j = va_arg(argList, VaType);
		data[IP++] = j;
		//printf(" %X",j);
	}
	P = TOBYTES(IP);
	va_end(argList);
}
void WHILE(int len, ...) {
	IP = TOWORDS(P);
	word_t k;
	//printf("\n%X WHILE ",P);
	data[IP++] = QBRAN;
	data[IP++] = 0;
	k = popR;
	pushR = (IP - 1);
	pushR = k;
	va_list argList;
	va_start(argList, len);
	for (; len; len--) {
		word_t j = va_arg(argList, VaType);
		data[IP++] = j;
		//printf(" %X",j);
	}
	P = TOBYTES(IP);
	va_end(argList);
}
void REPEAT(int len, ...) {
	IP = TOWORDS(P);
	//printf("\n%X REPEAT ",P);
	data[IP++] = BRAN;
	data[IP++] = TOBYTES(popR);
	data[popR] = TOBYTES(IP);
	va_list argList;
	va_start(argList, len);
	for (; len; len--) {
		word_t j = va_arg(argList, VaType);
		data[IP++] = j;
		//printf(" %X",j);
	}
	P = TOBYTES(IP);
	va_end(argList);
}
void IF(int len, ...) {
	IP = TOWORDS(P);
	//printf("\n%X IF ",P);
	data[IP++] = QBRAN;
	pushR = IP;
	data[IP++] = 0;
	va_list argList;
	va_start(argList, len);
	for (; len; len--) {
		word_t j = va_arg(argList, VaType);
		data[IP++] = j;
		//printf(" %X",j);
	}
	P = TOBYTES(IP);
	va_end(argList);
}
void ELSE(int len, ...) {
	IP = TOWORDS(P);
	//printf("\n%X ELSE ",P);
	data[IP++] = BRAN;
	data[IP++] = 0;
	data[popR] = TOBYTES(IP);
	pushR = IP - 1;
	va_list argList;
	va_start(argList, len);
	for (; len; len--) {
		word_t j = va_arg(argList, VaType);
		data[IP++] = j;
		//printf(" %X",j);
	}
	P = TOBYTES(IP);
	va_end(argList);
}
void THEN(int len, ...) {
	IP = TOWORDS(P);
	//printf("\n%X THEN ",P);
	data[popR] = TOBYTES(IP);
	va_list argList;
	va_start(argList, len);
	for (; len; len--) {
		word_t j = va_arg(argList, VaType);
		data[IP++] = j;
		//printf(" %X",j);
	}
	P = TOBYTES(IP);
	va_end(argList);
}
void FOR(int len, ...) {
	IP = TOWORDS(P);
	//printf("\n%X FOR ",P);
	data[IP++] = TOR;
	pushR = IP;
	va_list argList;
	va_start(argList, len);
	for (; len; len--) {
		word_t j = va_arg(argList, VaType);
		data[IP++] = j;
		//printf(" %X",j);
	}
	P = TOBYTES(IP);
	va_end(argList);
}
void NEXT(int len, ...) {
	IP = TOWORDS(P);
	//printf("\n%X NEXT ",P);
	data[IP++] = DONXT;
	data[IP++] = TOBYTES(popR);
	va_list argList;
	va_start(argList, len);
	for (; len; len--) {
		word_t j = va_arg(argList, VaType);
		data[IP++] = j;
		//printf(" %X",j);
	}
	P = TOBYTES(IP);
	va_end(argList);
}
void AFT(int len, ...) {
	IP = TOWORDS(P);
	word_t k;
	//printf("\n%X AFT ",P);
	data[IP++] = BRAN;
	data[IP++] = 0;
	k = popR;
	pushR = IP;
	pushR = IP - 1;
	va_list argList;
	va_start(argList, len);
	for (; len; len--) {
		word_t j = va_arg(argList, VaType);
		data[IP++] = j;
		//printf(" %X",j);
	}
	P = TOBYTES(IP);
	va_end(argList);
}
void DOTQ(const char seq[]) {
	IP = TOWORDS(P);
	int i;
	int len = strlen(seq);
	data[IP++] = DOTQP;
	P = TOBYTES(IP);
	cData[P++] = len;
	for (i = 0; i < len; i++)
	{
		cData[P++] = seq[i];
	}
        CODEALIGN;
	//printf("\n%X ",P);
	//printf(seq);
}
void STRQ(const char seq[]) {
	IP = TOWORDS(P);
	int i;
	int len = strlen(seq);
	data[IP++] = STRQP;
	P = TOBYTES(IP);
	cData[P++] = len;
	for (i = 0; i < len; i++)
	{
		cData[P++] = seq[i];
	}
        CODEALIGN;
	//printf("\n%X ",P);
	//printf(seq);
}
void ABORQ(const char seq[]) {
	IP = TOWORDS(P);
	int i;
	int len = strlen(seq);
	data[IP++] = ABORQP;
	P = TOBYTES(IP);
	cData[P++] = len;
	for (i = 0; i < len; i++)
	{
		cData[P++] = seq[i];
	}
        CODEALIGN;
	//printf("\n%X ",P);
	//printf(seq);
}

void CheckSum() {
	int i;
	char sum = 0;
	printf("\n%4X ", P);
	for (i = 0; i < 16; i++) {
		sum += cData[P];
		printf("%2X", cData[P++]);
	}
	printf(" %2X", sum & 0XFF);
}

/*
* Save image.
*/
void save (void)
{
        FILE *fout;
        int i;
        uword_t size;

        fout = fopen ("eforth.new", "w");
        size = TOWORDS(IZ);
        printf ("writing eforth.new size = %04X\n", IZ);
        fwrite (&size, sizeof(uword_t), 1, fout);
        fwrite (data, sizeof(word_t), size, fout);
/*
        for (i = 0; i < IP; i++)
# if BytesPerWord == 8
                fprintf (fout, "%04X %016llX\n", BytesPerWord * i, data[i]);
# else
                fprintf (fout, "%04X %08X\n", BytesPerWord * i, data[i]);
# endif
*/
        fclose (fout);
}

#else

/*
* Load image.
*/
void load (void)
{
        FILE *fin;
        uword_t size;

        fin = fopen ("eforth.img", "r");
        if (fin == NULL) {
                fprintf (stderr, "eforth.img not found!");
                exit (1);
        }
        fread (&size, sizeof(uword_t), 1, fin);
        printf ("loading eforth.img size = %04X\n", sizeof(word_t) * size);
        fread (data, sizeof(word_t), size, fin);
        fclose (fin);
}

#endif

/*
* Main Program
*/
int main(int ac, char* av[])
{
	cData = (unsigned char*)data;
#ifdef BOOT
	P = 512;
	R = 0;


	// Kernel

	HEADER(3, "HLD");
	word_t HLD =   CODE(3, as_clit, SYSVARBASE +  0*BytesPerWord, as_next);   
	HEADER(4, "SPAN");
	word_t SPAN =  CODE(3, as_clit, SYSVARBASE +  1*BytesPerWord, as_next);
	HEADER(3, ">IN");
	word_t INN =   CODE(3, as_clit, SYSVARBASE +  2*BytesPerWord, as_next);
	HEADER(4, "#TIB");
	word_t NTIB =  CODE(3, as_clit, SYSVARBASE +  3*BytesPerWord, as_next);
	HEADER(4, "'TIB");
	word_t TTIB =  CODE(3, as_clit, SYSVARBASE +  4*BytesPerWord, as_next);
	HEADER(4, "BASE");
	word_t BASE =  CODE(3, as_clit, SYSVARBASE +  5*BytesPerWord, as_next);
	HEADER(7, "CONTEXT");
	word_t CNTXT = CODE(3, as_clit, SYSVARBASE +  6*BytesPerWord, as_next);
	HEADER(2, "CP");
	word_t CP =    CODE(3, as_clit, SYSVARBASE +  7*BytesPerWord, as_next);
	HEADER(4, "LAST");
	word_t LAST =  CODE(3, as_clit, SYSVARBASE +  8*BytesPerWord, as_next);
	HEADER(5, "'EVAL");
	word_t TEVAL = CODE(3, as_clit, SYSVARBASE +  9*BytesPerWord, as_next);
	HEADER(6, "'ABORT");
	word_t TABRT = CODE(3, as_clit, SYSVARBASE + 10*BytesPerWord, as_next);
	HEADER(3, "tmp");
	word_t TEMP =  CODE(3, as_clit, SYSVARBASE + 11*BytesPerWord, as_next);

	HEADER(3, "NOP");
	word_t NOP = CODE(1, as_next);
	HEADER(3, "BYE");
	word_t BYE = CODE(2, as_bye, as_next);
	HEADER(3, "?RX");
	word_t QRX = CODE(4, as_clit, 0, as_sys, as_next);
	HEADER(3, "TX!");
	word_t TXSTO = CODE(4, as_clit, 1, as_sys, as_next);
	HEADER(5, "DOCON");
	word_t DOCON = CODE(2, as_docon, as_next);
	HEADER(5, "DOLIT");
	word_t DOLIT = CODE(2, as_dolit, as_next);
	HEADER(6, "DOLIST");
	word_t DOLST = CODE(2, as_dolist, as_next);
	HEADER(4, "EXIT");
	word_t EXITT = CODE(2, as_exitt, as_next);
	HEADER(7, "EXECUTE");
	word_t EXECU = CODE(2, as_execu, as_next);
	HEADER(6, "DONEXT");
	DONXT = CODE(2, as_donext, as_next);
	HEADER(7, "QBRANCH");
	QBRAN = CODE(2, as_qbran, as_next);
	HEADER(6, "BRANCH");
	BRAN = CODE(2, as_bran, as_next);
	HEADER(1, "!");
	word_t STORE = CODE(2, as_store, as_next);
	HEADER(1, "@");
	word_t AT = CODE(2, as_at, as_next);
	HEADER(2, "C!");
	word_t CSTOR = CODE(2, as_cstor, as_next);
	HEADER(2, "C@");
	word_t CAT = CODE(2, as_cat, as_next);
	HEADER(2, "R>");
	word_t RFROM = CODE(2, as_rfrom, as_next);
	HEADER(2, "R@");
	word_t RAT = CODE(2, as_rat, as_next);
	HEADER(2, ">R");
	TOR = CODE(2, as_tor, as_next);
	HEADER(4, "DROP");
	word_t DROP = CODE(2, as_drop, as_next);
	HEADER(3, "DUP");
	word_t DUPP = CODE(2, as_dup, as_next);
	HEADER(4, "SWAP");
	word_t SWAP = CODE(2, as_swap, as_next);
	HEADER(4, "OVER");
	word_t OVER = CODE(2, as_over, as_next);
	HEADER(2, "0<");
	word_t ZLESS = CODE(2, as_zless, as_next);
	HEADER(3, "AND");
	word_t ANDD = CODE(2, as_andd, as_next);
	HEADER(2, "OR");
	word_t ORR = CODE(2, as_orr, as_next);
	HEADER(3, "XOR");
	word_t XORR = CODE(2, as_xorr, as_next);
	HEADER(3, "UM+");
	word_t UPLUS = CODE(2, as_uplus, as_next);
	HEADER(4, "NEXT");
	word_t NEXTT = CODE(2, as_next, as_next);
	HEADER(4, "?DUP");
	word_t QDUP = CODE(2, as_qdup, as_next);
	HEADER(3, "ROT");
	word_t ROT = CODE(2, as_rot, as_next);
	HEADER(5, "2DROP");
	word_t DDROP = CODE(2, as_ddrop, as_next);
	HEADER(4, "2DUP");
	word_t DDUP = CODE(2, as_ddup, as_next);
	HEADER(1, "+");
	word_t PLUS = CODE(2, as_plus, as_next);
	HEADER(3, "NOT");
	word_t INVER = CODE(2, as_inver, as_next);
	HEADER(6, "NEGATE");
	word_t NEGAT = CODE(2, as_negat, as_next);
	HEADER(7, "DNEGATE");
	word_t DNEGA = CODE(2, as_dnega, as_next);
	HEADER(1, "-");
	word_t SUBBB = CODE(2, as_subb, as_next);
	HEADER(3, "ABS");
	word_t ABSS = CODE(2, as_abss, as_next);
	HEADER(1, "=");
	word_t EQUAL = CODE(2, as_equal, as_next);
	HEADER(2, "U<");
	word_t ULESS = CODE(2, as_uless, as_next);
	HEADER(1, "<");
	word_t LESS = CODE(2, as_less, as_next);
	HEADER(6, "UM/MOD");
	word_t UMMOD = CODE(2, as_ummod, as_next);
	HEADER(5, "M/MOD");
	word_t MSMOD = CODE(2, as_msmod, as_next);
	HEADER(4, "/MOD");
	word_t SLMOD = CODE(2, as_slmod, as_next);
	HEADER(3, "MOD");
	word_t MODD = CODE(2, as_mod, as_next);
	HEADER(1, "/");
	word_t SLASH = CODE(2, as_slash, as_next);
	HEADER(3, "UM*");
	word_t UMSTA = CODE(2, as_umsta, as_next);
	HEADER(1, "*");
	word_t STAR = CODE(2, as_star, as_next);
	HEADER(2, "M*");
	word_t MSTAR = CODE(2, as_mstar, as_next);
	HEADER(5, "*/MOD");
	word_t SSMOD = CODE(2, as_ssmod, as_next);
	HEADER(2, "*/");
	word_t STASL = CODE(2, as_stasl, as_next);
	HEADER(4, "PICK");
	word_t PICK = CODE(2, as_pick, as_next);
	HEADER(2, "+!");
	word_t PSTOR = CODE(2, as_pstor, as_next);
	HEADER(2, "2!");
	word_t DSTOR = CODE(2, as_dstor, as_next);
	HEADER(2, "2@");
	word_t DAT = CODE(2, as_dat, as_next);
	HEADER(5, "COUNT");
	word_t COUNT = CODE(2, as_count, as_next);
	HEADER(3, "MAX");
	word_t MAX = CODE(2, as_max, as_next);
	HEADER(3, "MIN");
	word_t MIN = CODE(2, as_min, as_next);
	HEADER(2, "BL");
	word_t BLANK = CODE(3, as_clit, 32, as_next);
	HEADER(4, "CELL");
	word_t CELL = CODE(3, as_clit, BytesPerWord, as_next);
	HEADER(5, "CELL+");
	word_t CELLP = CODE(4, as_clit, BytesPerWord, as_plus, as_next);
	HEADER(5, "CELL-");
	word_t CELLM = CODE(4, as_clit, BytesPerWord, as_subb, as_next);
	HEADER(5, "CELLS");
	word_t CELLS = CODE(4, as_clit, BytesPerWord, as_star, as_next);
	HEADER(5, "CELL/");
	word_t CELLD = CODE(4, as_clit, BytesPerWord, as_slash, as_next);
	HEADER(2, "1+");
	word_t ONEP = CODE(4, as_clit, 1, as_plus, as_next);
	HEADER(2, "1-");
	word_t ONEM = CODE(4, as_clit, 1, as_subb, as_next);
	HEADER(5, "DOVAR");
	word_t DOVAR = CODE(2, as_dovar, as_next);
	HEADER(2, "MS");
	word_t MSS = CODE(4, as_clit, 2, as_sys, as_next);

	// Common Colon Words

	HEADER(4, "?KEY");
	word_t QKEY = COLON(2, QRX, EXITT);
	HEADER(3, "KEY");
	word_t KEY = COLON(0);
	BEGIN(1, QKEY);
	UNTIL(1, EXITT);
	HEADER(4, "EMIT");
	word_t EMIT = COLON(2, TXSTO, EXITT);
	HEADER(6, "WITHIN");
	word_t WITHI = COLON(7, OVER, SUBBB, TOR, SUBBB, RFROM, ULESS, EXITT);
	HEADER(5, ">CHAR");
	word_t TCHAR = COLON(8, DOLIT, 0x7F, ANDD, DUPP, DOLIT, 0X7F, BLANK, WITHI);
	IF(3, DROP, DOLIT, 0X5F);
	THEN(1, EXITT);
	HEADER(7, "ALIGNED");
	word_t ALIGN = COLON(7, DOLIT, (BytesPerWord-1), PLUS, DOLIT, ~((uword_t)(BytesPerWord-1)), ANDD, EXITT);
	HEADER(4, "HERE");
	word_t HERE = COLON(3, CP, AT, EXITT);
	HEADER(3, "PAD");
	word_t PAD = COLON(5, HERE, DOLIT, 0X50, PLUS, EXITT);
	HEADER(3, "TIB");
	word_t TIB = COLON(3, TTIB, AT, EXITT);
	HEADER(8, "@EXECUTE");
	word_t ATEXE = COLON(2, AT, QDUP);
	IF(1, EXECU);
	THEN(1, EXITT);
	HEADER(5, "CMOVE");
	word_t CMOVEE = COLON(0);
	FOR(0);
	AFT(8, OVER, CAT, OVER, CSTOR, TOR, ONEP, RFROM, ONEP);
	THEN(0);
	NEXT(2, DDROP, EXITT);
	HEADER(4, "MOVE");
	word_t MOVE = COLON(1, CELLD);
	FOR(0);
	AFT(8, OVER, AT, OVER, STORE, TOR, CELLP, RFROM, CELLP);
	THEN(0);
	NEXT(2, DDROP, EXITT);
	HEADER(4, "FILL");
	word_t FILL = COLON(1, SWAP);
	FOR(1, SWAP);
	AFT(3, DDUP, CSTOR, ONEP);
	THEN(0);
	NEXT(2, DDROP, EXITT);

	// Number Conversions

	HEADER(5, "DIGIT");
	word_t DIGIT = COLON(12, DOLIT, 9, OVER, LESS, DOLIT, 7, ANDD, PLUS, DOLIT, 0X30, PLUS, EXITT);
	HEADER(7, "EXTRACT");
	word_t EXTRC = COLON(7, DOLIT, 0, SWAP, UMMOD, SWAP, DIGIT, EXITT);
	HEADER(2, "<#");
	word_t BDIGS = COLON(4, PAD, HLD, STORE, EXITT);
	HEADER(4, "HOLD");
	word_t HOLD = COLON(8, HLD, AT, ONEM, DUPP, HLD, STORE, CSTOR, EXITT);
	HEADER(1, "#");
	word_t DIG = COLON(5, BASE, AT, EXTRC, HOLD, EXITT);
	HEADER(2, "#S");
	word_t DIGS = COLON(0);
	BEGIN(2, DIG, DUPP);
	WHILE(0);
	REPEAT(1, EXITT);
	HEADER(4, "SIGN");
	word_t SIGN = COLON(1, ZLESS);
	IF(3, DOLIT, 0X2D, HOLD);
	THEN(1, EXITT);
	HEADER(2, "#>");
	word_t EDIGS = COLON(7, DROP, HLD, AT, PAD, OVER, SUBBB, EXITT);
	HEADER(3, "str");
	word_t STRR = COLON(9, DUPP, TOR, ABSS, BDIGS, DIGS, RFROM, SIGN, EDIGS, EXITT);
	HEADER(3, "HEX");
	word_t HEXX = COLON(5, DOLIT, 16, BASE, STORE, EXITT);
	HEADER(7, "DECIMAL");
	word_t DECIM = COLON(5, DOLIT, 10, BASE, STORE, EXITT);
	HEADER(6, "wupper");
	word_t UPPER = COLON(4, DOLIT, UpperMask, ANDD, EXITT);
	HEADER(6, ">upper");
	word_t TOUPP = COLON(6, DUPP, DOLIT, 0x61, DOLIT, 0x7B, WITHI);
	IF(3, DOLIT, 0x5F, ANDD);
	THEN(1, EXITT);
	HEADER(6, "DIGIT?");
	word_t DIGTQ = COLON(9, TOR, TOUPP, DOLIT, 0X30, SUBBB, DOLIT, 9, OVER, LESS);
	IF(8, DOLIT, 7, SUBBB, DUPP, DOLIT, 10, LESS, ORR);
	THEN(4, DUPP, RFROM, ULESS, EXITT);
	HEADER(7, "NUMBER?");
	word_t NUMBQ = COLON(12, BASE, AT, TOR, DOLIT, 0, OVER, COUNT, OVER, CAT, DOLIT, 0X24, EQUAL);
	IF(5, HEXX, SWAP, ONEP, SWAP, ONEM);
	THEN(13, OVER, CAT, DOLIT, 0X2D, EQUAL, TOR, SWAP, RAT, SUBBB, SWAP, RAT, PLUS, QDUP);
	IF(1, ONEM);
	FOR(6, DUPP, TOR, CAT, BASE, AT, DIGTQ);
	WHILE(7, SWAP, BASE, AT, STAR, PLUS, RFROM, ONEP);
	NEXT(2, DROP, RAT);
	IF(1, NEGAT);
	THEN(1, SWAP);
	ELSE(6, RFROM, RFROM, DDROP, DDROP, DOLIT, 0);
	THEN(1, DUPP);
	THEN(6, RFROM, DDROP, RFROM, BASE, STORE, EXITT);

	// Terminal Output

        HEADER(4, "nuf?");
        word_t NUFQ = COLON(2, QKEY, DUPP);
        IF(5, DDROP, KEY, DOLIT, 0XD, EQUAL);
        THEN(1, EXITT);
	HEADER(5, "SPACE");
	word_t SPACE = COLON(3, BLANK, EMIT, EXITT);
	HEADER(5, "CHARS");
	word_t CHARS = COLON(4, SWAP, DOLIT, 0, MAX);
	FOR(0);
	AFT(2, DUPP, EMIT);
	THEN(0);
	NEXT(2, DROP, EXITT);
	HEADER(6, "SPACES");
	word_t SPACS = COLON(3, BLANK, CHARS, EXITT);
	HEADER(4, "TYPE");
	word_t TYPES = COLON(0);
	FOR(0);
	AFT(3, COUNT, TCHAR, EMIT);
	THEN(0);
	NEXT(2, DROP, EXITT);
	HEADER(2, "CR");
	word_t CR = COLON(7, DOLIT, 10, DOLIT, 13, EMIT, EMIT, EXITT);
	HEADER(3, "do$");
	word_t DOSTR = COLON(10, RFROM, RAT, RFROM, COUNT, PLUS, ALIGN, TOR, SWAP, TOR, EXITT);
	HEADER(3, "$\"|");
	word_t STRQP = COLON(2, DOSTR, EXITT);
	HEADER(3, ".\"|");
	DOTQP = COLON(4, DOSTR, COUNT, TYPES, EXITT);
	HEADER(2, ".R");
	word_t DOTR = COLON(8, TOR, STRR, RFROM, OVER, SUBBB, SPACS, TYPES, EXITT);
	HEADER(3, "U.R");
	word_t UDOTR = COLON(10, TOR, BDIGS, DIGS, EDIGS, RFROM, OVER, SUBBB, SPACS, TYPES, EXITT);
	HEADER(2, "U.");
	word_t UDOT = COLON(6, BDIGS, DIGS, EDIGS, SPACE, TYPES, EXITT);
	HEADER(1, ".");
	word_t DOT = COLON(5, BASE, AT, DOLIT, 0XA, XORR);
	IF(2, UDOT, EXITT);
	THEN(4, STRR, SPACE, TYPES, EXITT);
	HEADER(1, "?");
	word_t QUEST = COLON(3, AT, DOT, EXITT);

	// Parser

	HEADER(7, "(parse)");
	word_t PARS = COLON(5, TEMP, CSTOR, OVER, TOR, DUPP);
	IF(5, ONEM, TEMP, CAT, BLANK, EQUAL);
	IF(0);
	FOR(6, BLANK, OVER, CAT, SUBBB, ZLESS, INVER);
	WHILE(1, ONEP);
	NEXT(6, RFROM, DROP, DOLIT, 0, DUPP, EXITT);
	THEN(1, RFROM);
	THEN(2, OVER, SWAP);
	FOR(9, TEMP, CAT, OVER, CAT, SUBBB, TEMP, CAT, BLANK, EQUAL);
	IF(1, ZLESS);
	THEN(0);
	WHILE(1, ONEP);
	NEXT(2, DUPP, TOR);
	ELSE(5, RFROM, DROP, DUPP, ONEP, TOR);
	THEN(6, OVER, SUBBB, RFROM, RFROM, SUBBB, EXITT);
	THEN(4, OVER, RFROM, SUBBB, EXITT);
        /* XXX */
	HEADER(5, "PACK$");
	word_t PACKS = COLON(18, DUPP, TOR, DDUP, PLUS, DOLIT, ~((uword_t)(BytesPerWord-1)), ANDD, DOLIT, 0, SWAP, STORE, DDUP, CSTOR, ONEP, SWAP, CMOVEE, RFROM, EXITT);
	HEADER(5, "PARSE");
	word_t PARSE = COLON(15, TOR, TIB, INN, AT, PLUS, NTIB, AT, INN, AT, SUBBB, RFROM, PARS, INN, PSTOR, EXITT);
	HEADER(5, "TOKEN");
	word_t TOKEN = COLON(9, BLANK, PARSE, DOLIT, 0x1F, MIN, HERE, CELLP, PACKS, EXITT);
	HEADER(4, "WORD");
	word_t WORDD = COLON(5, PARSE, HERE, CELLP, PACKS, EXITT);
	HEADER(5, "NAME>");
	word_t NAMET = COLON(7, COUNT, DOLIT, 0x1F, ANDD, PLUS, ALIGN, EXITT);
	HEADER(5, "SAME?");
	word_t SAMEQ = COLON(4, DOLIT, 0x1F, ANDD, CELLD);
	FOR(0);
	AFT(14, OVER, RAT, CELLS, PLUS, AT, UPPER, OVER, RAT, CELLS, PLUS, AT, UPPER, SUBBB, QDUP);
	IF(3, RFROM, DROP, EXITT);
	THEN(0);
	THEN(0);
	NEXT(3, DOLIT, 0, EXITT);
	HEADER(4, "find");
	word_t FIND = COLON(10, SWAP, DUPP, AT, TEMP, STORE, DUPP, AT, TOR, CELLP, SWAP);
	BEGIN(2, AT, DUPP);
	IF(9, DUPP, AT, DOLIT, (word_t)-193/*0xFFF..3F*/, ANDD, UPPER, RAT, UPPER, XORR);
	IF(3, CELLP, DOLIT, (word_t)-1);
	ELSE(4, CELLP, TEMP, AT, SAMEQ);
	THEN(0);
	ELSE(6, RFROM, DROP, SWAP, CELLM, SWAP, EXITT);
	THEN(0);
	WHILE(2, CELLM, CELLM);
	REPEAT(9, RFROM, DROP, SWAP, DROP, CELLM, DUPP, NAMET, SWAP, EXITT);
	HEADER(5, "NAME?");
	word_t NAMEQ = COLON(3, CNTXT, FIND, EXITT);

	// Terminal Input

	HEADER(2, "^H");
	word_t HATH = COLON(6, TOR, OVER, RFROM, SWAP, OVER, XORR);
	IF(9, DOLIT, 8, EMIT, ONEM, BLANK, EMIT, DOLIT, 8, EMIT);
	THEN(1, EXITT);
	HEADER(3, "TAP");
	word_t TAP = COLON(6, DUPP, EMIT, OVER, CSTOR, ONEP, EXITT);
	HEADER(4, "kTAP");
	word_t KTAP = COLON(9, DUPP, DOLIT, 0XD, XORR, OVER, DOLIT, 0XA, XORR, ANDD);
	IF(3, DOLIT, 8, XORR);
	IF(2, BLANK, TAP);
	ELSE(1, HATH);
	THEN(1, EXITT);
	THEN(5, DROP, SWAP, DROP, DUPP, EXITT);
	HEADER(6, "ACCEPT");
	word_t ACCEP = COLON(3, OVER, PLUS, OVER);
	BEGIN(2, DDUP, XORR);
	WHILE(7, KEY, DUPP, BLANK, SUBBB, DOLIT, 0X5F, ULESS);
	IF(1, TAP);
	ELSE(1, KTAP);
	THEN(0);
	REPEAT(4, DROP, OVER, SUBBB, EXITT);
	HEADER(6, "EXPECT");
	word_t EXPEC = COLON(5, ACCEP, SPAN, STORE, DROP, EXITT);
	HEADER(5, "QUERY");
	word_t QUERY = COLON(12, TIB, DOLIT, 0X50, ACCEP, NTIB, STORE, DROP, DOLIT, 0, INN, STORE, EXITT);

	// Text Interpreter

	HEADER(5, "ABORT");
	word_t ABORT = COLON(2, TABRT, ATEXE);
	HEADER(6, "abort\"");
	ABORQP = COLON(0);
	IF(4, DOSTR, COUNT, TYPES, ABORT);
	THEN(3, DOSTR, DROP, EXITT);
	HEADER(5, "ERROR");
	word_t ERRORR = COLON(11, SPACE, COUNT, TYPES, DOLIT, 0x3F, EMIT, DOLIT, 0X1B, EMIT, CR, ABORT);
	HEADER(10, "$INTERPRET");
	word_t INTER = COLON(2, NAMEQ, QDUP);
	IF(4, CAT, DOLIT, COMPO, ANDD);
	ABORQ(" compile only");
	word_t INTER0 = LABEL(2, EXECU, EXITT);
	THEN(1, NUMBQ);
	IF(1, EXITT);
	ELSE(1, ERRORR);
	THEN(0);
	HEADER(IMEDD + 1, "[");
	word_t LBRAC = COLON(5, DOLIT, INTER, TEVAL, STORE, EXITT);
	HEADER(3, ".OK");
	word_t DOTOK = COLON(6, CR, DOLIT, INTER, TEVAL, AT, EQUAL);
	IF(14, TOR, TOR, TOR, DUPP, DOT, RFROM, DUPP, DOT, RFROM, DUPP, DOT, RFROM, DUPP, DOT);
	DOTQ(" ok>");
	THEN(1, EXITT);
	HEADER(4, "EVAL");
	word_t EVAL = COLON(0);
	BEGIN(3, TOKEN, DUPP, AT);
	WHILE(2, TEVAL, ATEXE);
	REPEAT(3, DROP, DOTOK, EXITT);
	HEADER(4, "QUIT");
	word_t QUITT = COLON(5, DOLIT, 0X100, TTIB, STORE, LBRAC);
	BEGIN(2, QUERY, EVAL);
	AGAIN(0);

	// Colon Word Compiler

	HEADER(1, ",");
	word_t COMMA = COLON(7, HERE, DUPP, CELLP, CP, STORE, STORE, EXITT);
	HEADER(IMEDD + 7, "LITERAL");
	word_t LITER = COLON(5, DOLIT, DOLIT, COMMA, COMMA, EXITT);
	HEADER(5, "ALLOT");
	word_t ALLOT = COLON(4, ALIGN, CP, PSTOR, EXITT);
	HEADER(3, "$,\"");
	word_t STRCQ = COLON(9, DOLIT, 0X22, WORDD, COUNT, PLUS, ALIGN, CP, STORE, EXITT);
	HEADER(7, "?UNIQUE");
	word_t UNIQU = COLON(3, DUPP, NAMEQ, QDUP);
	IF(6, COUNT, DOLIT, 0x1F, ANDD, SPACE, TYPES);
	DOTQ(" reDef");
	THEN(2, DROP, EXITT);
	HEADER(3, "$,n");
	word_t SNAME = COLON(2, DUPP, AT);
	IF(14, UNIQU, DUPP, NAMET, CP, STORE, DUPP, LAST, STORE, CELLM, CNTXT, AT, SWAP, STORE, EXITT);
	THEN(1, ERRORR);
	HEADER(1, "'");
	word_t TICK = COLON(2, TOKEN, NAMEQ);
	IF(1, EXITT);
	THEN(1, ERRORR);
	HEADER(IMEDD + 9, "[COMPILE]");
	word_t BCOMP = COLON(3, TICK, COMMA, EXITT);
	HEADER(7, "COMPILE");
	word_t COMPI = COLON(7, RFROM, DUPP, AT, COMMA, CELLP, TOR, EXITT);
	HEADER(8, "$COMPILE");
	word_t SCOMP = COLON(2, NAMEQ, QDUP);
	IF(4, AT, DOLIT, IMEDD, ANDD);
	IF(1, EXECU);
	ELSE(1, COMMA);
	THEN(1, EXITT);
	THEN(1, NUMBQ);
	IF(2, LITER, EXITT);
	THEN(1, ERRORR);
	HEADER(5, "OVERT");
	word_t OVERT = COLON(5, LAST, AT, CNTXT, STORE, EXITT);
	HEADER(1, "]");
	word_t RBRAC = COLON(5, DOLIT, SCOMP, TEVAL, STORE, EXITT);
	HEADER(1, ":");
	word_t COLN = COLON(7, TOKEN, SNAME, RBRAC, DOLIT, 0x6, COMMA, EXITT);
	HEADER(IMEDD + 1, ";");
	word_t SEMIS = COLON(6, DOLIT, EXITT, COMMA, LBRAC, OVERT, EXITT);

	// Debugging Tools

	HEADER(3, "dm+");
	word_t DMP = COLON(4, OVER, DOLIT, 6, UDOTR);
	FOR(0);
	AFT(6, DUPP, AT, DOLIT, 1 + BytesPerWord * 2, UDOTR, CELLP);
	THEN(0);
	NEXT(1, EXITT);
	HEADER(4, "DUMP");
	word_t DUMP = COLON(10, BASE, AT, TOR, HEXX, DOLIT, 0x1F, PLUS, DOLIT, 0x20, SLASH);
	FOR(0);
	AFT(10, CR, DOLIT, 32 / BytesPerWord, DDUP, DMP, TOR, SPACE, CELLS, TYPES, RFROM);
	THEN(0);
	NEXT(5, DROP, RFROM, BASE, STORE, EXITT);
	HEADER(5, ">NAME");
	word_t TNAME = COLON(1, CNTXT);
	BEGIN(2, AT, DUPP);
	WHILE(3, DDUP, NAMET, XORR);
	IF(1, CELLM);
	ELSE(3, SWAP, DROP, EXITT);
	THEN(0);
	REPEAT(3, SWAP, DROP, EXITT);
	HEADER(3, ".ID");
	word_t DOTID = COLON(7, COUNT, DOLIT, 0x1F, ANDD, TYPES, SPACE, EXITT);
	HEADER(5, "WORDS");
	word_t WWORDS = COLON(6, CR, CNTXT, DOLIT, 0, TEMP, STORE);
	BEGIN(2, AT, QDUP);
	WHILE(9, DUPP, SPACE, DOTID, CELLM, TEMP, AT, DOLIT, 0xA, LESS);
	IF(4, DOLIT, 1, TEMP, PSTOR);
	ELSE(5, CR, DOLIT, 0, TEMP, STORE);
	THEN(0);
	REPEAT(1, EXITT);
        HEADER(3, "SEE");
        word_t SEE = COLON(3, TICK, CR, CELLP);
        BEGIN(4, ONEP, DUPP, AT, DUPP);
        IF(1, TNAME);
        THEN(1, QDUP);
        IF(4, SPACE, DOTID, ONEM, CELLP);
        ELSE(3, DUPP, CAT, UDOT);
        THEN(4, DOLIT, 100, MSS, NUFQ);
        UNTIL(2, DROP, EXITT);
	HEADER(6, "FORGET");
	word_t FORGT = COLON(3, TOKEN, NAMEQ, QDUP);
	IF(12, CELLM, DUPP, CP, STORE, AT, DUPP, CNTXT, STORE, LAST, STORE, DROP, EXITT);
	THEN(1, ERRORR);
	HEADER(4, "COLD");
	word_t COLD = COLON(1, CR);
	DOTQ("eForth in C,Ver 2.3,2017 ");
	word_t DOTQ1 = LABEL(2, CR, QUITT);

	// Structure Compiler

	HEADER(IMEDD + 4, "THEN");
	word_t THENN = COLON(4, HERE, SWAP, STORE, EXITT);
	HEADER(IMEDD + 3, "FOR");
	word_t FORR = COLON(4, COMPI, TOR, HERE, EXITT);
	HEADER(IMEDD + 5, "BEGIN");
	word_t BEGIN = COLON(2, HERE, EXITT);
	HEADER(IMEDD + 4, "NEXT");
	word_t NEXT = COLON(4, COMPI, DONXT, COMMA, EXITT);
	HEADER(IMEDD + 5, "UNTIL");
	word_t UNTIL = COLON(4, COMPI, QBRAN, COMMA, EXITT);
	HEADER(IMEDD + 5, "AGAIN");
	word_t AGAIN = COLON(4, COMPI, BRAN, COMMA, EXITT);
	HEADER(IMEDD + 2, "IF");
	word_t IFF = COLON(7, COMPI, QBRAN, HERE, DOLIT, 0, COMMA, EXITT);
	HEADER(IMEDD + 5, "AHEAD");
	word_t AHEAD = COLON(7, COMPI, BRAN, HERE, DOLIT, 0, COMMA, EXITT);
	HEADER(IMEDD + 6, "REPEAT");
	word_t REPEA = COLON(3, AGAIN, THENN, EXITT);
	HEADER(IMEDD + 3, "AFT");
	word_t AFT = COLON(5, DROP, AHEAD, HERE, SWAP, EXITT);
	HEADER(IMEDD + 4, "ELSE");
	word_t ELSEE = COLON(4, AHEAD, SWAP, THENN, EXITT);
	HEADER(IMEDD + 4, "WHEN");
	word_t WHEN = COLON(3, IFF, OVER, EXITT);
	HEADER(IMEDD + 5, "WHILE");
	word_t WHILEE = COLON(3, IFF, SWAP, EXITT);
	HEADER(IMEDD + 6, "ABORT\"");
	word_t ABRTQ = COLON(6, DOLIT, ABORQP, HERE, STORE, STRCQ, EXITT);
	HEADER(IMEDD + 2, "$\"");
	word_t STRQ = COLON(6, DOLIT, STRQP, HERE, STORE, STRCQ, EXITT);
	HEADER(IMEDD + 2, ".\"");
	word_t DOTQQ = COLON(6, DOLIT, DOTQP, HERE, STORE, STRCQ, EXITT);
	HEADER(4, "CODE");
	word_t CODE = COLON(4, TOKEN, SNAME, OVERT, EXITT);
	HEADER(6, "CREATE");
	word_t CREAT = COLON(5, CODE, DOLIT, 0x203D, COMMA, EXITT);
	HEADER(8, "VARIABLE");
	word_t VARIA = COLON(5, CREAT, DOLIT, 0, COMMA, EXITT);
	HEADER(8, "CONSTANT");
	word_t CONST = COLON(6, CODE, DOLIT, 0x2004, COMMA, COMMA, EXITT);
	HEADER(IMEDD + 2, ".(");
	word_t DOTPR = COLON(5, DOLIT, 0X29, PARSE, TYPES, EXITT);
	HEADER(IMEDD + 1, "\\");
	word_t BKSLA = COLON(5, DOLIT, 0xA, WORDD, DROP, EXITT);
	HEADER(IMEDD + 1, "(");
	word_t PAREN = COLON(5, DOLIT, 0X29, PARSE, DDROP, EXITT);
	HEADER(12, "COMPILE-ONLY");
	word_t ONLY = COLON(6, DOLIT, 0x40, LAST, AT, PSTOR, EXITT);
	HEADER(9, "IMMEDIATE");
        //  L F A:4  9 I M M E D I A T E 0 0 CFA:4 PFA:...
	word_t IMMED = COLON(6, DOLIT, 0x80, LAST, AT, PSTOR, EXITT);
	word_t ENDD = P;

	// Boot Up

	printf("\n\nIZ=%X R-stack=%X", P, (popR << 2));
        IZ = P;
	P = 0;
	word_t RESET = LABEL(2, as_dolist, COLD);
	P = SYSVARBASE + BytesPerWord * 4;
	word_t USER = LABEL(8, 
                         0x100,                                 // TIB
                         0x10,                                  // BASE
                         IMMED - Round(1 + 9),                  // CONTEXT: IMMEDIATE's NFA
                         ENDD,                                  // CP
                         IMMED - Round(1 + 9),                  // LAST: IMMEDIATE's NFA
                         INTER,                                 // 'EVAL
                         QUITT,                                 // 'ABORT
                         0);                                    // temp

	// dump dictionary
	//P = 0;
	//for (len = 0; len < 0x200; len++) { CheckSum(); }

        printf ("\nSaving ceForth v3.3, 01jul19cht\n");
        save ();

#ifndef STANDALONE
        exit (0);
#endif

#else
        load ();
#endif

	printf("\nceForth v3.3, 01jul19cht\n");

#ifdef _CURTERM_H
        if (isatty(fileno(stdin))) {
                prepterm(1);
                kbflush();
        }
#endif

#ifdef STC
        stc ();
#else
	P = 0;
	WP = 4;
	IP = 0;
	S = 0;
	R = 0;
	top = 0;
	while (TRUE) {
		primitives[(unsigned char)cData[P++]]();
	}
#endif
        return 0;
}
/* End of ceforth_33.cpp */

