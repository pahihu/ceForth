( 500M loops --- mhx 22may2020 )

\ i5 @ 1.8GHz (max. 2.8GHz)
\ iForth6  1052  185
\ SF3      1159  356
\ Gforth   1225 1444
\ FiCL     1405 5757

\ iMac 27" Core2 Duo @ 3.06GHz
\ SF3      1031  346    1377
\ iForth6  1014  506    1520
\ Gforth    916 1078    1994    fast
\ Gforth   1207 3900    5107
\ FiCL     2362 5912    8274

decimal
: 500Mloops 500000000 for next ;
: bench1 timer-reset 500mloops .elapsed ;

( iForth does not remember that RBX is unchanged... )
: 500Mloops2 500000000 begin 1 - dup 0 = until drop ;
: bench2 timer-reset 500mloops2 .elapsed ;

: FIB ( n1 -- n2)
   DUP 2 < IF DROP 1 EXIT THEN
   1- DUP RECURSE SWAP 1- RECURSE + 1+ ;

: BENCHFIB ( -- )
   timer-reset  38 fib drop  .elapsed ;

: -ROT ( x y z -- z x y )   ROT ROT ;
: 2DUP ( a b - a b a b)   OVER OVER ;
: 2DROP ( a b)   DROP DROP ;
: 2SWAP ( a b c d - c d a b)   ROT >R ROT R> ;
: 2OVER ( a b c d - a b c d a b)   >R >R 2DUP R> R> 2SWAP ;
: 3DUP ( x y z -- x y z x y z )   DUP 2OVER ROT ;

: TAK ( z y x -- n )
   2dup < 0 = IF  2drop exit  then
        3dup 1- recurse >R
   -rot 3dup 1- recurse >R
   -rot      1- recurse R> R> recurse ;

bench1 bench2 benchFib bye
