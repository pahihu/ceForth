( System interface)
HEX
CODE >ABS ( a - A)       020203 ,
CODE >REL ( A - a)       020303 ,
CODE DLSYM ( Zstr - A)   020403 ,
CODE CALLC ( aN ... a1 N fn - ret)   020503 ,


( C interface)
: doCALLC   R>  2@ CALLC ;

: +NUL ( a)   COUNT +  0 SWAP C! ;

: CRESOLVE ( <name> - fn)
   >IN @ >R
      BL WORD  DUP +NUL  COUNT DROP >ABS DLSYM
      DUP 0 = ABORT" not found"
   R> >IN ! ;

: FUNCTION: ( nargs <name>)
   CRESOLVE : COMPILE doCALLC , , [COMPILE] ; ;


1 FUNCTION: usleep

DECIMAL
: MS ( n)   1000 * usleep drop ;


2 FUNCTION: gettimeofday
CREATE tv   2 CELLS ALLOT

HEX
: TOD ( - usec sec)
   0 tv >ABS gettimeofday drop
   tv 2@  >R  0ffffffff AND  R> ;

: UNUSED ( - u)   [ 4000 CELLS ] LITERAL HERE - ;
