( Timing)
DECIMAL
VARIABLE T0
VARIABLE DIFF0

: COUNTER ( - n)   TOD 1000 * SWAP 1000 / + ;
: (TIMER) ( n1 - n2)   COUNTER SWAP - ;
: TIMER ( n1)   (TIMER) U. ;

: TIMER-RESET ( -- )   COUNTER T0 ! ;
: .ELAPSED ( -- )   COUNTER T0 @ -  DUP DIFF0 !  U. ." ms" ;
