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

# include <stdlib.h>
# include <stdio.h>
# ifndef __APPLE__
#  include <tchar.h>
# endif
# include <stdarg.h>
# include <string.h>
# include <stdint.h>
# ifdef _WIN32
#  define BYTE_ORDER 1234
#  define WIN32_LEAN_AND_MEAN 1
#  include <windows.h>
# else
#  include <unistd.h>
#  include <dlfcn.h>
# endif
# ifdef USE_CURTERM
#  include "curterm.h"
# endif

# define BPW 8
# define CYCLIC_STACK

# if BPW==8

typedef int64_t word_t;
typedef uint64_t uword_t;
typedef int64_t  dword_t;
typedef uint64_t udword_t;
# define TOWORDS(x) ((x) >> 3)
# define TOBYTES(x) ((x) << 3)
# define UpperMask  ((uword_t)0x5F5F5F5F5F5F5F5FULL)
# define VaType     word_t

# elif BPW==4

typedef int32_t word_t;
typedef uint32_t uword_t;
typedef int64_t  dword_t;
typedef uint64_t udword_t;
# define TOWORDS(x) ((x) >> 2)
# define TOBYTES(x) ((x) << 2)
# define UpperMask  ((uword_t)0x5F5F5F5FUL)
# define VaType     word_t

# elif BPW==2

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

# if BYTE_ORDER==1234
#  define       MKWORD(c1,c2)   ((c1)+((c2)<<8))
# else
#  define       MKWORD(c1,c2)   (((c1)<<(8*(BPW-1)))+((c2)<<(8*(BPW-2))))
# endif

# define        Round(x)   (((x) + BPW-1) & ~(uword_t)(BPW-1))

# define        MEM_SIZE   (16*1024)
# define        STACK_SIZE 256
# define        RACK_SIZE  256
# ifdef CYCLIC_STACK
#  define       RX(x)      (( RACK_SIZE-1)&(x))
#  define       SX(x)      ((STACK_SIZE-1)&(x))
# else
#  define       RX(x)      (x)
#  define       SX(x)      (x)
# endif

# define	FALSE	0
# define	TRUE	-1
# define	LOGICAL ? TRUE : FALSE
# define 	LOWER(x,y) ((uword_t)(x)<(uword_t)(y))
# define	pop	top = stack[SX(S--)]
# define	push	stack[SX(++S)] = top; top =
# define	popR    I = rack[RX(R--)]
# define	pushR   rack[RX(++R)] = I; I =

# define        CODEALIGN   while (P & (BPW-1)) { cData[P++] = 0; }
# define        SYSVARS     0X80


word_t  IZ, thread;

word_t data[MEM_SIZE] = {0};
unsigned char* cData = (unsigned char*)data;

# ifdef STC
word_t I, *rack = data + MEM_SIZE - RACK_SIZE;
int R = 0;
word_t  P, IP;
# endif

# define do_opsys(x) \
        x(qrx)   x(txsto)  x(toabs) x(torel) x(dlsym)  x(callc)

# define do_codes(x) \
        x(nop)   x(bye)    x(sys)   x(clit)  x(docon) x(dolit) x(dolist) x(exitt) \
        x(execu) x(donext) x(qbran) x(bran)  x(store) x(at)    x(cstor)  x(cat)   \
        x(rpat)  x(rpsto)  x(rfrom) x(rat)   x(tor)   x(spat)  x(spsto)  x(drop)  \
        x(dup)   x(swap)   x(over)  x(zless) x(andd)  x(orr)   x(xorr)   x(uplus) \
        x(next)  x(qdup)   x(rot)   x(ddrop) x(ddup)  x(plus)  x(inver)  x(negat) \
        x(dnega) x(subb)   x(abss)  x(equal) x(uless) x(less)  x(ummod)  x(msmod) \
        x(slmod) x(mod)    x(slash) x(umsta) x(star)  x(mstar) x(ssmod)  x(stasl) \
        x(pick)  x(pstor)  x(dstor) x(dat)   x(count) x(dovar) x(max)    x(min)

// Byte Code Assembler

# define def_as(x)       as_##x,
enum { do_codes(def_as)  as_numbytecodes } ByteCodes;

# define def_sys(x)      sys_##x,
enum { do_opsys(def_sys) sys_numsyscalls } SysCalls;


# ifdef STC
static void stc(void) {
# else
#  define do_decl(x)      static void P##x(void);
do_codes(do_decl)
# endif

dword_t d, n, m;
word_t *rack;
word_t *stack;
int R = 0;
int S = 0;
word_t  I = 0, top = 0;
word_t  P, IP, WP, UP;
unsigned char bytecode, c;

# ifdef GCC_DISPATCH
#  define do_label(x)     &&L##x,
        static void *dispatch[] = {
                do_codes(do_label)
                0
        };
# endif


# define macNEXT         P = data[TOWORDS(IP)]; WP = P + BPW; IP += BPW;
# define macDROP         pop
# define macOVER 	 push stack[SX(S - 1)];
# define macINVER	 top = -top - 1;
# define macTOR          pushR top; pop;
# define macUPLUS        stack[SX(S)] += top; top = LOWER(stack[SX(S)], top);
# define macRFROM        push I; popR;
# define macPLUS         top += stack[SX(S--)];

# ifdef STC 
#  define FETCH_CODE            bytecode = (unsigned char)cData[P++];
#  define doNEXT                macNEXT
#  ifdef GCC_DISPATCH
#   define PRIMITIVE(x)         L##x: {
#   ifdef __clang__
#    define PEND                FETCH_CODE; goto *dispatch[bytecode]; }
#   else
#    define PEND                FETCH_CODE; goto *dispatch[bytecode]; }
#   endif
#  else
#   define PRIMITIVE(x)         case as_##x: {
#   define PEND                 FETCH_CODE; } break;
#  endif
# else
#  define PRIMITIVE(x)          static void P##x(void) {
#  define PEND                  }
#  define doNEXT                Pnext()
# endif

// Virtual Forth Machine
# ifdef STC
	P = 0;
	WP = BPW;
	IP = 0;
	top = 0;
        I   = 0;

        rack   = data + MEM_SIZE - RACK_SIZE;
        stack  = rack - STACK_SIZE;
        *stack = 0; S = 0;
        *rack  = 0; R = 0;

        FETCH_CODE;
#  ifdef GCC_DISPATCH
        goto *dispatch[bytecode];
#  else
        while (TRUE) {
                switch (bytecode) {
#  endif
# endif

PRIMITIVE(bye)
# ifdef USE_CURTERM
        prepterm(0);
# endif
	exit(0);
PEND
PRIMITIVE(sys)
        WP = top; pop;
        switch (WP) {
        case sys_qrx: /* ?RX ( - c t|0) */
# ifdef USE_CURTERM
                if (isatty(fileno(stdin))) {
                        if (has_key()) {
                                push(word_t) getkey();
                                kbflush();
                        } else {
                                push 0;
                        }
                } else
# endif
                {
                        push(word_t) getchar();
                }
	        if (top != 0) {
                        push TRUE;
                }
                break;
        case sys_txsto: /* TX! ( c) */
                putchar((char)top);
                if (isatty(fileno(stdout)))
                        fflush(stdout);
	        pop;
                break;
        case sys_toabs: /* >ABS ( a - A) */
                top = (word_t)(cData + top);
                break;
        case sys_torel: /* >REL ( A - a) */
                top = ((unsigned char *)top - cData);
                break;
        case sys_dlsym: /* DLSYM ( A - A|0) */
# ifdef _WIN32
                top = (word_t) GetProcAddress(GetModuleHandle(NULL), (char *)top);
# else
                top = (word_t) dlsym(RTLD_DEFAULT, (char *)top);
# endif
                break;
        case sys_callc: /* (CALL) ( argN ... arg1 N fn -- ret ) */ {
	        long (*fn)();
	        int i, narg;
	        long arg[8];
	        long ret;

	        fn = (long (*)()) top; pop;
	        narg = top; pop;

	        for (i = 0; i < narg; i++) {
		        arg[i] = top; pop;
	        }

	        switch (narg) {
	        case 0: ret = (*fn)(); break;
	        case 1: ret = (*fn)(arg[0]); break;
	        case 2: ret = (*fn)(arg[0], arg[1]); break;
	        case 3: ret = (*fn)(arg[0], arg[1], arg[2]); break;
	        case 4: ret = (*fn)(arg[0], arg[1], arg[2], arg[3]); break;
	        case 5: ret = (*fn)(arg[0], arg[1], arg[2], arg[3], arg[4]); break;
	        case 6: ret = (*fn)(arg[0], arg[1], arg[2], arg[3], arg[4], arg[5]); break;
	        case 7: ret = (*fn)(arg[0], arg[1], arg[2], arg[3], arg[4], arg[5], arg[6]); break;
	        default:
		        break;
	        }

                push(word_t) ret;
                } break;
        default:
                printf("unknown SYS %X\n", WP);
        }
PEND
PRIMITIVE(next)
        macNEXT;
PEND

PRIMITIVE(dovar)
	push WP;
PEND
PRIMITIVE(docon)
	push data[TOWORDS(WP)];
PEND
PRIMITIVE(dolit)
	push data[TOWORDS(IP)];
	IP += BPW;
	doNEXT;
PEND
PRIMITIVE(dolist)
        pushR IP;
	IP = WP;
	doNEXT;
PEND
PRIMITIVE(exitt)
	IP = I; popR;
	doNEXT;
PEND
PRIMITIVE(execu)
	P = top;
	WP = P + BPW;
	pop;
PEND
PRIMITIVE(donext)
	if (I) {
		I--;
		IP = data[TOWORDS(IP)];
	}
	else {
		IP += BPW;
                popR;
	}
	doNEXT;
PEND
PRIMITIVE(qbran)
	if (top == 0) IP = data[TOWORDS(IP)];
	else IP += BPW;
	pop;
	doNEXT;
PEND
PRIMITIVE(bran)
	IP = data[TOWORDS(IP)];
	doNEXT;
PEND
PRIMITIVE(store)
	data[TOWORDS(top)] = stack[SX(S--)];
	pop;
PEND
PRIMITIVE(at)
	top = data[TOWORDS(top)];
PEND
PRIMITIVE(cstor)
	cData[top] = (char)stack[SX(S--)];
	pop;
PEND
PRIMITIVE(cat)
	top = (word_t)cData[top];
PEND
PRIMITIVE(rpat)
        WP = rack - data;
        WP = RACK_SIZE * WP + R;
        push WP;
PEND
PRIMITIVE(rpsto)
        WP = top; pop;
        R  = WP % RACK_SIZE; WP /= RACK_SIZE;
        rack = data + WP;
PEND
PRIMITIVE(rfrom)
        macRFROM;
PEND
PRIMITIVE(rat)
	push I;
PEND
PRIMITIVE(tor)
        macTOR;
PEND
PRIMITIVE(spat)
        WP = (stack - data);
        WP = STACK_SIZE * WP + S;
        push WP;
PEND
PRIMITIVE(spsto)
        WP = top; pop;
        S  = WP % STACK_SIZE; WP /= STACK_SIZE;
        stack = data + WP;
PEND
PRIMITIVE(drop)
        macDROP;
PEND
PRIMITIVE(dup)
	stack[SX(++S)] = top;
PEND
PRIMITIVE(swap)
	WP = top;
	top = stack[SX(S)];
	stack[SX(S)] = WP;
PEND
PRIMITIVE(over)
        macOVER;
PEND
PRIMITIVE(zless)
	top = (top < 0) LOGICAL;
PEND
PRIMITIVE(andd)
	top &= stack[SX(S--)];
PEND
PRIMITIVE(orr)
	top |= stack[SX(S--)];
PEND
PRIMITIVE(xorr)
	top ^= stack[SX(S--)];
PEND
PRIMITIVE(uplus)
        macUPLUS;
PEND
PRIMITIVE(nop)
	doNEXT;
PEND
PRIMITIVE(qdup)
	if (top) stack[SX(++S)] = top;
PEND
PRIMITIVE(rot)
	WP = stack[SX(S - 1)];
	stack[SX(S - 1)] = stack[SX(S)];
	stack[SX(S)] = top;
	top = WP;
PEND
PRIMITIVE(ddrop)
	macDROP; macDROP;
PEND
PRIMITIVE(ddup)
        macOVER; macOVER;
PEND
PRIMITIVE(plus)
        macPLUS;
PEND
PRIMITIVE(inver)
        macINVER;
PEND
PRIMITIVE(negat)
	top = 0 - top;
PEND
PRIMITIVE(dnega)
        macINVER;
        macTOR;
        macINVER;
	push 1;
        macUPLUS;
	macRFROM;
	macPLUS;
PEND
PRIMITIVE(subb)
	top = stack[SX(S--)] - top;
PEND
PRIMITIVE(abss)
	if (top < 0)
		top = -top;
PEND
/*
PRIMITIVE(great)
	top = (stack[SX(S--)] > top) LOGICAL;
PEND
*/
PRIMITIVE(less)
	top = (stack[SX(S--)] < top) LOGICAL;
PEND
PRIMITIVE(equal)
	top = (stack[SX(S--)] == top) LOGICAL;
PEND
PRIMITIVE(uless)
	top = LOWER(stack[SX(S)], top) LOGICAL; S--;
PEND
PRIMITIVE(ummod)
	d = (dword_t)((uword_t)top);
	m = (dword_t)((uword_t)stack[SX(S)]);
	n = (dword_t)((uword_t)stack[SX(S - 1)]);
# if BPW != 8
	n += m << (8*BPW);
# endif
	pop;
	top = (uword_t)(n / d);
	stack[SX(S)] = (uword_t)(n % d);
PEND
PRIMITIVE(msmod)
	d = (dword_t)((word_t)top);
	m = (dword_t)((word_t)stack[SX(S)]);
	n = (dword_t)((word_t)stack[SX(S - 1)]);
# if BPW != 8
	n += m << (8*BPW);
# endif
	pop;
	top = (word_t)(n / d);
	stack[SX(S)] = (word_t)(n % d);
PEND
PRIMITIVE(slmod)
	if (top != 0) {
		WP = stack[SX(S)] / top;
		stack[SX(S)] %= top;
		top = WP;
	}
PEND
PRIMITIVE(mod)
	top = (top) ? stack[SX(S--)] % top : stack[SX(S--)];
PEND
PRIMITIVE(slash)
	top = (top) ? stack[SX(S--)] / top : (stack[SX(S--)], 0);
PEND
PRIMITIVE(umsta)
	d = (udword_t)top;
	m = (udword_t)stack[SX(S)];
	m *= d;
# if BPW == 8
        top = 0;
# else
	top = (uword_t)(m >> (8*BPW));
# endif
	stack[SX(S)] = (uword_t)m;
PEND
PRIMITIVE(star)
	top *= stack[SX(S--)];
PEND
PRIMITIVE(mstar)
	d = (dword_t)top;
	m = (dword_t)stack[SX(S)];
	m *= d;
# if BPW == 8
        top = 0;
# else
	top = (word_t)(m >> (8*BPW));
# endif
	stack[SX(S)] = (word_t)m;
PEND
PRIMITIVE(ssmod)
	d = (dword_t)top;
	m = (dword_t)stack[SX(S)];
	n = (dword_t)stack[SX(S - 1)];
	n *= m;
	pop;
	top = (word_t)(n / d);
	stack[SX(S)] = (word_t)(n % d);
PEND
PRIMITIVE(stasl)
	d = (dword_t)top;
	m = (dword_t)stack[SX(S)];
	n = (dword_t)stack[SX(S - 1)];
	n *= m;
	pop; pop;
	top = (word_t)(n / d);
PEND
PRIMITIVE(pick)
	top = stack[SX(S - top)];
PEND
PRIMITIVE(pstor)
	data[TOWORDS(top)] += stack[SX(S--)], pop;
PEND
PRIMITIVE(dstor)
        top = TOWORDS(top);
	data[top] = stack[SX(S--)];
	data[top + 1] = stack[SX(S--)];
	pop;
PEND
PRIMITIVE(dat)
        WP = TOWORDS(top);
	top = data[WP + 1];
	push data[WP];
PEND
PRIMITIVE(count)
	stack[SX(++S)] = top + 1;
	top = cData[top];
PEND
PRIMITIVE(max)
	if (top < stack[SX(S)]) pop;
	else S--;
PEND
PRIMITIVE(min)
	if (top < stack[SX(S)]) S--;
	else pop;
PEND
PRIMITIVE(clit)
        push (unsigned char)cData[P++];
PEND


# ifdef STC
#  ifdef GCC_DISPATCH
        }
#  else
        }}}
#  endif
# endif

# ifndef STC
void(*primitives[as_numbytecodes])(void) = {
	/* case 0 */ Pnop,
	/* case 1 */ Pbye,
	/* case 2 */ Psys,
	/* case 3 */ Pclit,
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
	/* case 16 */ Prpat,
	/* case 17 */ Prpsto,
	/* case 18 */ Prfrom,
	/* case 19 */ Prat,
	/* case 20 */ Ptor,
	/* case 21 */ Pspat,
	/* case 22 */ Pspsto,
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
	/* case 63 */ Pmin
};
# endif

# ifdef BOOT
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
void C0MMA(word_t j) {
        IP = TOWORDS(P);
        data[IP++] = j;
        P = TOBYTES(IP);
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

#define C0MPILEC \
	va_list argList; \
	va_start(argList, len); \
	for (; len; len--) { \
		word_t j = va_arg(argList, VaType); \
		data[IP++] = j; \
		/*printf(" %X",j)*/; \
	} \
	P = TOBYTES(IP); \
	va_end(argList);

word_t COLON(int len, ...) {
	word_t addr = P;
	IP = TOWORDS(P);
	data[IP++] = as_dolist; // dolist
        C0MPILEC;
	return addr;
}
word_t LABEL(int len, ...) {
	word_t addr = P;
	IP = TOWORDS(P);
        C0MPILEC;
	return addr;
}
void BEGIN(int len, ...) {
	IP = TOWORDS(P);
	//printf("\n%X BEGIN ",P);
	pushR IP;
        C0MPILEC;
}
void AGAIN(int len, ...) {
	IP = TOWORDS(P);
	//printf("\n%X AGAIN ",P);
	data[IP++] = BRAN;
	data[IP++] = TOBYTES(I); popR;
        C0MPILEC;
}
void UNTIL(int len, ...) {
	IP = TOWORDS(P);
	//printf("\n%X UNTIL ",P);
	data[IP++] = QBRAN;
	data[IP++] = TOBYTES(I); popR;
        C0MPILEC;
}
void WHILE(int len, ...) {
	IP = TOWORDS(P);
	word_t k;
	//printf("\n%X WHILE ",P);
	data[IP++] = QBRAN;
	data[IP++] = 0;
	k = I; popR;
	pushR (IP - 1);
	pushR k;
        C0MPILEC;
}
void REPEAT(int len, ...) {
	IP = TOWORDS(P);
	//printf("\n%X REPEAT ",P);
	data[IP++] = BRAN;
	data[IP++] = TOBYTES(I); popR;
	data[I] = TOBYTES(IP); popR;
        C0MPILEC;
}
void IF(int len, ...) {
	IP = TOWORDS(P);
	//printf("\n%X IF ",P);
	data[IP++] = QBRAN;
	pushR IP;
	data[IP++] = 0;
        C0MPILEC;
}
void ELSE(int len, ...) {
	IP = TOWORDS(P);
	//printf("\n%X ELSE ",P);
	data[IP++] = BRAN;
	data[IP++] = 0;
	data[I] = TOBYTES(IP); popR;
	pushR (IP - 1);
        C0MPILEC;
}
void THEN(int len, ...) {
	IP = TOWORDS(P);
	//printf("\n%X THEN ",P);
	data[I] = TOBYTES(IP); popR;
        C0MPILEC;
}
void FOR(int len, ...) {
	IP = TOWORDS(P);
	//printf("\n%X FOR ",P);
	data[IP++] = TOR;
	pushR IP;
        C0MPILEC;
}
void NEXT(int len, ...) {
	IP = TOWORDS(P);
	//printf("\n%X NEXT ",P);
	data[IP++] = DONXT;
	data[IP++] = TOBYTES(I); popR;
        C0MPILEC;
}
void AFT(int len, ...) {
	IP = TOWORDS(P);
	word_t k;
	//printf("\n%X AFT ",P);
	data[IP++] = BRAN;
	data[IP++] = 0;
	k = I; popR;
	pushR IP;
	pushR (IP - 1);
        C0MPILEC;
}
void CSTRCQ(word_t p, const char seq[]) {
	IP = TOWORDS(P);
	int i;
	int len = strlen(seq);
	data[IP++] = p;
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
void DOTQ(const char seq[]) {
        CSTRCQ(DOTQP, seq);
}
void STRQ(const char seq[]) {
        CSTRCQ(STRQP, seq);
}
void ABORQ(const char seq[]) {
        CSTRCQ(ABORQP, seq);
}

void CheckSum() {
	int i;
	char sum = 0;
	printf("\n%04X ", P);
	for (i = 0; i < 16; i++) {
		sum += cData[P];
		printf("%02X", cData[P++]);
	}
	printf(" %02X", sum & 0XFF);
}

/*
* Save image.
*/
void save (void)
{
        FILE *fout;
        int i;
        uword_t size;

        fout = fopen ("eforth.new", "wb");
        size = TOWORDS(IZ);
        printf ("writing eforth.new size = %04X\n", IZ);
        fwrite (&size, sizeof(uword_t), 1, fout);
        fwrite (data, sizeof(word_t), size, fout);
/*
        for (i = 0; i < IP; i++)
# if BPW == 8
                fprintf (fout, "%04X %016llX\n", BPW * i, data[i]);
# else
                fprintf (fout, "%04X %08X\n", BPW * i, data[i]);
# endif
*/
        fclose (fout);
}

# else

/*
* Load image.
*/
void load (void)
{
        FILE *fin;
        uword_t size;

        fin = fopen ("eforth.img", "rb");
        if (fin == NULL) {
                fprintf (stderr, "eforth.img not found!");
                exit (1);
        }
        fread (&size, sizeof(uword_t), 1, fin);
        printf ("loading eforth.img size = %04X\n", sizeof(word_t) * size);
        fread (data, sizeof(word_t), size, fin);
        fclose (fin);
}

# endif

/*
* Main Program
*/
int main(int ac, char* av[])
{
	cData = (unsigned char*)data;
        rack  = data + MEM_SIZE - RACK_SIZE;

# ifdef BOOT
        I = 0; *rack = 0; R = 0;
	P = 512;

	// Kernel

	HEADER(3, "HLD");
	word_t HLD =   CODE(2, as_docon, as_next); C0MMA(SYSVARS +  0*BPW);
	HEADER(4, "SPAN");
	word_t SPAN =  CODE(2, as_docon, as_next); C0MMA(SYSVARS +  1*BPW);
	HEADER(3, ">IN");
	word_t INN =   CODE(2, as_docon, as_next); C0MMA(SYSVARS +  2*BPW);
	HEADER(4, "#TIB");
	word_t NTIB =  CODE(2, as_docon, as_next); C0MMA(SYSVARS +  3*BPW);
	HEADER(4, "'TIB");
	word_t TTIB =  CODE(2, as_docon, as_next); C0MMA(SYSVARS +  4*BPW);
	HEADER(4, "BASE");
	word_t BASE =  CODE(2, as_docon, as_next); C0MMA(SYSVARS +  5*BPW);
	HEADER(7, "CONTEXT");
	word_t CNTXT = CODE(2, as_docon, as_next); C0MMA(SYSVARS +  6*BPW);
	HEADER(2, "CP");
	word_t CP =    CODE(2, as_docon, as_next); C0MMA(SYSVARS +  7*BPW);
	HEADER(4, "LAST");
	word_t LAST =  CODE(2, as_docon, as_next); C0MMA(SYSVARS +  8*BPW);
	HEADER(5, "'EVAL");
	word_t TEVAL = CODE(2, as_docon, as_next); C0MMA(SYSVARS +  9*BPW);
	HEADER(6, "'ABORT");
	word_t TABRT = CODE(2, as_docon, as_next); C0MMA(SYSVARS + 10*BPW);
	HEADER(3, "tmp");
	word_t TEMP =  CODE(2, as_docon, as_next); C0MMA(SYSVARS + 11*BPW);

	HEADER(3, "NOP");
	word_t NOP = CODE(1, as_next);
	HEADER(3, "BYE");
	word_t BYE = CODE(2, as_bye, as_next);
	HEADER(3, "?RX");
	word_t QRX = CODE(4, as_clit, sys_qrx, as_sys, as_next);
	HEADER(3, "TX!");
	word_t TXSTO = CODE(4, as_clit, sys_txsto, as_sys, as_next);
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
	word_t CELL = CODE(3, as_clit, BPW, as_next);
	HEADER(5, "CELL+");
	word_t CELLP = CODE(4, as_clit, BPW, as_plus, as_next);
	HEADER(5, "CELL-");
	word_t CELLM = CODE(4, as_clit, BPW, as_subb, as_next);
	HEADER(5, "CELLS");
	word_t CELLS = CODE(4, as_clit, BPW, as_star, as_next);
	HEADER(5, "CELL/");
	word_t CELLD = CODE(4, as_clit, BPW, as_slash, as_next);
	HEADER(2, "1+");
	word_t ONEP = CODE(4, as_clit, 1, as_plus, as_next);
	HEADER(2, "1-");
	word_t ONEM = CODE(4, as_clit, 1, as_subb, as_next);
	HEADER(5, "DOVAR");
	word_t DOVAR = CODE(2, as_dovar, as_next);

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
	word_t ALIGN = COLON(7, DOLIT, (BPW-1), PLUS, DOLIT, ~((uword_t)(BPW-1)), ANDD, EXITT);
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
	word_t PACKS = COLON(18, DUPP, TOR, DDUP, PLUS, DOLIT, ~((uword_t)(BPW-1)), ANDD, DOLIT, 0, SWAP, STORE, DDUP, CSTOR, ONEP, SWAP, CMOVEE, RFROM, EXITT);
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
	word_t COLN = COLON(7, TOKEN, SNAME, RBRAC, DOLIT, MKWORD(as_dolist,as_nop), COMMA, EXITT);
	HEADER(IMEDD + 1, ";");
	word_t SEMIS = COLON(6, DOLIT, EXITT, COMMA, LBRAC, OVERT, EXITT);

	// Debugging Tools

	HEADER(3, "dm+");
	word_t DMP = COLON(4, OVER, DOLIT, 6, UDOTR);
	FOR(0);
	AFT(6, DUPP, AT, DOLIT, 1 + BPW * 2, UDOTR, CELLP);
	THEN(0);
	NEXT(1, EXITT);
	HEADER(4, "DUMP");
	word_t DUMP = COLON(10, BASE, AT, TOR, HEXX, DOLIT, 0x1F, PLUS, DOLIT, 0x20, SLASH);
	FOR(0);
	AFT(10, CR, DOLIT, 32 / BPW, DDUP, DMP, TOR, SPACE, CELLS, TYPES, RFROM);
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
	word_t CREAT = COLON(6, CODE, DOLIT, MKWORD(as_dovar,as_next), COMMA, EXITT);
	HEADER(8, "VARIABLE");
	word_t VARIA = COLON(5, CREAT, DOLIT, 0, COMMA, EXITT);
	HEADER(8, "CONSTANT");
	word_t CONSTT = COLON(7, CODE, DOLIT, MKWORD(as_docon,as_next), COMMA, COMMA, EXITT);
	HEADER(IMEDD + 2, ".(");
	word_t DOTPR = COLON(5, DOLIT, 0X29, PARSE, TYPES, EXITT);
	HEADER(IMEDD + 1, "\\");
	word_t BKSLA = COLON(5, DOLIT, 0XA, WORDD, DROP, EXITT);
	HEADER(IMEDD + 1, "(");
	word_t PAREN = COLON(5, DOLIT, 0X29, PARSE, DDROP, EXITT);
	HEADER(12, "COMPILE-ONLY");
	word_t ONLY = COLON(6, DOLIT, 0x40, LAST, AT, PSTOR, EXITT);
	HEADER(9, "IMMEDIATE");
        //  L F A:4  9 I M M E D I A T E 0 0 CFA:4 PFA:...
	word_t IMMED = COLON(6, DOLIT, 0x80, LAST, AT, PSTOR, EXITT);
	word_t ENDD = P;

	// Boot Up

	printf("\n\nIZ=%X R-stack=%X", P, (I << 2)); popR;
        IZ = P;
	P = 0;
	word_t RESET = LABEL(2, as_dolist, COLD);
	P = SYSVARS + 4 * BPW;
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
	P = 0;
	for (int len = 0; len < (IZ + 15) >> 4; len++) { CheckSum(); }

        printf ("\nSaving ceForth v3.3, 01jul19cht\n");
        save ();

#  ifndef STANDALONE
        exit (0);
#  endif

# else
        load ();
# endif

	printf("\nceForth v3.3, 01jul19cht\n");

# ifdef USE_CURTERM
        if (isatty(fileno(stdin))) {
                prepterm(1);
                kbflush();
        }
# endif

# ifdef STC
        stc ();
# else
        stack = rack - STACK_SIZE;

	P = 0;
	WP = 4;
	IP = 0;
        *stack = 0; S = 0;
        *rack  = 0; R = 0;
	top = 0;
        I   = 0;
	while (TRUE) {
		primitives[(unsigned char)cData[P++]]();
	}
# endif
        return 0;
}
/* End of ceforth_33.cpp */

