( eForth V2.3 -- from ceForth V3.3 ap6apr2020 )

( Kernel ===================================================== )

$80 CONSTANT IMEDD      ( IMMEDIATE flag)
$40 CONSTANT COMPO      ( COMPILE-ONLY flag)

$80 CONSTANT HLD
$84 CONSTANT SPAN
$88 CONSTANT >IN
$8C CONSTANT #TIB
$90 CONSTANT 'TIB       ( $100)
$94 CONSTANT BASE       (  $10)
$98 CONSTANT CONTEXT    ( nfa of IMMEDIATE)
$9C CONSTANT CP         ( 1st free entry in data[])
$A0 CONSTANT LAST       ( nfa of IMMEDIATE)
$A4 CONSTANT 'EVAL      ( cfa of $INTERPRET)
$A8 CONSTANT 'ABORT     ( cfa of ABORT")
$AC CONSTANT tmp        ( 0)

CODE NOP   END-CODE
CODE BYE   bye, END-CODE
CODE ?RX ( - c t|0)   qrx, END-CODE
CODE TX! ( c)   txsto, END-CODE
CODE DOCON ( - n)   docon, END-CODE
CODE DOLIT ( - n)   dolit, END-CODE
CODE DOLIST dolist, END-CODE
CODE EXIT   exitt, END-CODE
CODE EXECUTE ( a)   execu, END-CODE
CODE DONEXT  donext, END-CODE
CODE QBRANCH ( f)   qbran, END-CODE
CODE BRANCH  bran, END-CODE
CODE ! ( n a)   store, END-CODE
CODE @ ( a - n)   at, END-CODE
CODE C! ( c a)   cstor, END-CODE
CODE C@ ( a - c)   cat, END-CODE
CODE R> ( -n)   rfrom, END-CODE
CODE R@ ( -n)   rat, END-CODE
CODE >R ( n)   tor, END-CODE
CODE DROP ( n)   drop, END-CODE
CODE DUP ( n - n n)   dup, END-CODE
CODE SWAP ( a b - b a)   swap, END-CODE
CODE OVER ( a b - a b a)   over, END-CODE
CODE 0< ( n - f)   zless, END-CODE
CODE AND ( n1 n2 - n3)   andd, END-CODE
CODE OR ( n1 n2 - n3)   orr, END-CODE
CODE XOR ( n1 n2 - n3)   xorr, END-CODE
CODE UM+ ( u1 u2 - ud)   uplus, END-CODE
CODE NEXT    next, END-CODE
CODE ?DUP ( n - n n|0)   qdup, END-CODE
CODE ROT ( a b c - b c a)   rot, END-CODE
CODE 2DROP ( d)   ddrop, END-CODE
CODE 2DUP ( d - d d)   ddup, END-CODE
CODE + ( n1 n2 - n3)   plus, END-CODE
CODE NOT ( n - ~n)   inver, END-CODE
CODE NEGATE ( n - -n)   negat, END-CODE
CODE DNEGATE ( d - -d)   dnega, END-CODE
CODE - ( n1 n2 - n3)   subb, END-CODE
CODE ABS ( n - u)   abss, END-CODE
CODE = ( n1 n2 - f)   equal, END-CODE
CODE U< ( u1 u2 - f)   uless, END-CODE
CODE < ( n1 n2 - f)   less, END-CODE
CODE UM/MOD ( ud u1 - mod quo)   ummod, END-CODE
CODE M/MOD ( d n - mod quo)   msmod, END-CODE
CODE /MOD ( n1 n2 - mod quo)   slmod, END-CODE
CODE MOD ( n1 n2 - mod)   mod, END-CODE
CODE / ( n1 n2 - n3)   slash, END-CODE
CODE UM* ( u1 u2 - ud)   umsta, END-CODE
CODE * ( n1 n2 - n3)   star, END-CODE
CODE M* ( n1 n2 - d)   mstar, END-CODE
CODE */MOD ( n1 n2 n3 - mod quo)   ssmod, END-CODE
CODE */ ( n1 n2 n3 - quo)   stasl, END-CODE
CODE PICK ( n - n1)   pick, END-CODE
CODE +! ( n a)   pstor, END-CODE
CODE 2! ( d a)   dstor, END-CODE
CODE 2@ ( a - d)   dat, END-CODE
CODE COUNT ( a - a+1 #)   count, END-CODE
CODE MAX ( n1 n2 - n3)   max, END-CODE
CODE MIN ( n1 n2 - n3)   min, END-CODE

32 CONSTANT BL
 4 CONSTANT CELL
: CELL+ ( n - n+C)   CELL + ;
: CELL- ( n - n-C)   CELL - ;
: CELLS ( n - n*C)   CELL * ;
: CELL/ ( n - n/C)   CELL / ;
: 1+ ( n - n+1)   1 + ;
: 1- ( n - n-1)   1 - ;
CODE DOVAR ( - a)   dovar, END-CODE


( Common Colon Words ========================================= )

: ?KEY ( - c t|0)   ?RX ;
: KEY ( - c)  BEGIN ?KEY UNTIL ;
: EMIT ( c)   TX! ;
: WITHIN ( n lo hi - f)   OVER - >R - R> U< ;
: >CHAR ( c1 - c2)   $7F AND DUP $7F BL WITHIN IF DROP $5F THEN ;
: ALIGNED ( a1 - a2)   3 + $FFFFFFFC AND ;
: HERE ( - a)   CP @ ;
: PAD ( - a)   HERE $50 + ;
: TIB ( - a)   'TIB @ ;
: @EXECUTE ( a)   @ ?DUP IF EXECUTE THEN ;
: CMOVE ( s d #) FOR AFT  OVER C@  OVER C!  >R 1+ R> 1+  THEN NEXT  2DROP ;
: MOVE ( s d #)
   CELL/ FOR AFT  OVER @ OVER !  >R CELL+ R> CELL+  THEN NEXT  2DROP ;
: FILL ( a n c)   SWAP FOR SWAP  AFT  2DUP C!  1+ THEN NEXT  2DROP ;


( Number Conversion ========================================== )

: DIGIT ( u - c)   9 OVER < 7 AND + $30 + ;
: EXTRACT ( n1 base - n2 c)   0 SWAP UM/MOD SWAP DIGIT ;
: <#  PAD HLD ! ;
: HOLD ( c)   HLD @ 1-  DUP HLD !  C! ;
: # ( n1 - n2)   BASE @ EXTRACT HOLD ;
: #S ( n)   BEGIN # DUP WHILE REPEAT ;
: SIGN ( n)   0< IF $2D HOLD THEN ;
: #> ( n - a n)   DROP HLD @ PAD OVER - ;
: str ( n - a)   DUP >R ABS <# #S R> SIGN #> ;
: HEX  $10 BASE ! ;
: DECIMAL  $0A BASE ! ;
: wupper  $5F5F5F5F AND ;
: >upper  DUP $61 $7B WITHIN IF $5F AND THEN ;
: DIGIT? ( c base - n f)   >R >upper $30 - 9 OVER <
   IF 7 - DUP $0A < OR THEN
   DUP R> U< ;
: NUMBER? ( a - n t|a 0)   BASE @ >R 0 OVER COUNT OVER C@ $24 =
   IF HEX SWAP 1+ SWAP 1- THEN
   OVER C@ $2D = >R SWAP R@ - SWAP R@ + ?DUP
   IF 1- 
      FOR DUP >R C@ BASE @ DIGIT?
         WHILE SWAP BASE @ * + R> 1+
      NEXT DROP @
      IF NEGATE THEN  SWAP
   ELSE R> R> 2DROP 2DROP 0
   THEN DUP
   THEN R> 2DROP R> BASE ! ;


( Terminal Output ============================================ )

: SPACE  BL EMIT ;
: CHARS  SWAP 0 MAX FOR AFT DUP EMIT THEN NEXT DROP ;
: SPACES ( n)   BLANK CHARS ;
: TYPE ( a #)   FOR AFT COUNT >CHAR EMIT THEN NEXT DROP ;
: CR  $10 $13 EMIT EMIT ;
: do$ ( - a)   R> R@ R> COUNT + ALIGNED >R SWAP >R ;
: $"| ( - a)   do$ ;
: ."| ( - a)   do$ COUNT TYPE ;
: .R ( n #)   >R str R> OVER - SPACES TYPE ;
: U.R ( u #)   >R <# #S #> R> OVER - SPACES TYPE ;
: U. ( u)   <# #S #> SPACE TYPE ;
: . ( n)   BASE @ $0A XOR IF U. EXIT THEN str SPACE TYPE ;
: ? ( a)   @ . ;


( Parser ===================================================== )

: (parse) ( a # c - a n delta)   tmp C! OVER >R DUP
   IF 1- tmp C@ BL =
      IF FOR BL OVER C@ - 0< NOT
      WHILE 1+
      NEXT R> DROP 0 DUP EXIT
      THEN R>
   THEN OVER SWAP
   FOR tmp C@ OVER C@ - tmp C@ BL =
   IF 0< THEN
   WHILE 1+
   NEXT DUP >R
   ELSE R> DROP DUP 1+ >R
   THEN OVER - R> R> - EXIT
   THEN OVER R> - ;
: PACK$ ( s # d)
   DUP >R  2DUP + $FFFFFFFC AND  0 SWAP !  2DUP C!  1+ SWAP CMOVE  R> ;
: PARSE ( c - a #)   >R  TIB >IN @ +  #TIB @ >IN @ -  R>  (parse)  >IN +! ;
: TOKEN ( a)   BL PARSE  $1F MIN  HERE CELL+  PACK$ ;
: WORD ( c - a)   PARSE HERE CELL+ PACK$ ;
: NAME> ( nfa - cfa)   COUNT $1F AND + ALIGNED ;
: SAME? ( a1 a2 # - a1 a2 f)   $1F AND CELL/
   FOR AFT  OVER R@ CELLS + @ wupper  OVER R@ CELLS + @ wupper  -
      ?DUP IF R> DROP EXIT THEN
   THEN NEXT  0 ;
: find ( a va - cfa nfa|a 0)
   SWAP  DUP @ tmp !  DUP @ >R  CELL+ SWAP
   BEGIN
      @ DUP
      IF DUP @ $FFFFFF3F AND wupper  R@ wupper XOR
         IF   CELL+ $FFFFFFFF
         ELSE CELL+ tmp @ SAME?
         THEN
      ELSE  R> DROP  SWAP CELL- SWAP  EXIT
      THEN
   WHILE   CELL- CELL-
   REPEAT  R> DROP  SWAP DROP  CELL- DUP NAME>  SWAP ;
: NAME? ( a - cfa nfa|a 0)   CONTEXT find ;


( Terminal Input ============================================= )

: ^H ( bot eot cur - bot eot cur)
   >R OVER R> SWAP OVER XOR
   IF  8 EMIT  1-  BL EMIT  8 EMIT  THEN ;
: TAP ( bot eot cur c - bot eot cur)   DUP EMIT OVER C! 1+ ;
: kTAP ( bot eot cur c - bot eot cur)
   DUP  $0D XOR  OVER  $0A XOR  AND
   IF 8 XOR
      IF   BL TAP
      ELSE ^H
      THEN
      EXIT
   THEN  DROP SWAP DROP DUP ;
: ACCEPT ( a # - #)   OVER + OVER
   BEGIN 2DUP XOR
   WHILE KEY DUP BL - $5F U<
      IF   TAP
      ELSE kTAP
      THEN
   REPEAT  DROP OVER - ;
: EXPECT  ACCEPT SPAN ! DROP ;
: QUERY  TIB $50 ACCEPT #TIB ! DROP 0 >IN ! ;


( Text Interpreter =========================================== )

: ABORT  'ABORT @EXECUTE ; RECOVER
: abort" ( f)   IF  do$ COUNT TYPE ABORT  THEN  do$ DROP ;
: ERROR  SPACE COUNT TYPE $3F EMIT $1B EMIT CR ABORT ; RECOVER
: $INTERPRET ( a)
   NAME? ?DUP 
   IF C@ COMPO AND ABORT" compile only"
      EXECUTE EXIT
   THEN
   NUMBER? IF   EXIT
           ELSE ERROR
           THEN ; RECOVER
: [  ['] $INTERPRET 'EVAL ! ; IMMEDIATE
: .OK  CR ['] $INTERPRET 'EVAL @ =
   IF >R >R >R  DUP . R>  DUP . R>  DUP . R> DUP . ." ok>" THEN ;
: EVAL  BEGIN TOKEN DUP @ WHILE 'EVAL @EXECUTE REPEAT DROP .OK ;
: QUIT  $100 'TIB ! [COMPILE] [
   BEGIN QUERY EVAL AGAIN ;


( Colon Word Compiler ======================================== )

: , ( n)   HERE DUP CELL+ CP ! ! ;
: LITERAL ( n)   ['] DOLIT , , ; IMMEDIATE
: ALLOT ( n)   ALIGNED CP +! ;
: $,"  $22 WORD COUNT + ALIGNED CP ! ;
: ?UNIQUE ( a - a)
   DUP NAME? ?DUP
   IF   COUNT $1F AND SPACE TYPE ." reDef"  THEN  DROP ;
: $,n ( a)   DUP @ 
   IF  UNIQUE? DUP NAME> CP ! DUP LAST ! CELL- CONTEXT @ SWAP ! EXIT  THEN
   ERROR ; RECOVER
: ' ( <name> - a)   TOKEN NAME?  IF  EXIT  THEN  ERROR ; RECOVER
: [COMPILE] ( <name>)   ' , ; IMMEDIATE
: COMPILE ( <name>)   R> DUP @ , CELL+ >R ;
: $COMPILE ( a)
   NAME? ?DUP
   IF @ IMEDD AND
      IF   EXECUTE
      ELSE ,
      THEN  EXIT
   THEN  NUMBER?
   IF  LITERAL EXIT  THEN  ERROR ; RECOVER
: OVERT  LAST @ CONTEXT ! ;
: ]  ['] $COMPILE 'EVAL ! ;
: : ( <name>)  TOKEN $,n ] $06 , ; ( dolist, )
: ;  ['] EXIT , [COMPILE] [ OVERT ; IMMEDIATE


( Debugging Tools ============================================ )
: dm+  OVER 6 U.R FOR  AFT DUP @ 9 U.R CELL+ THEN  NEXT ;
: DUMP ( a #)
   BASE @ >R HEX $1F + $20 /
   FOR  AFT CR 8 2DUP dm+ >R SPACE CELLS TYPE R> THEN  NEXT
   DROP R> BASE ! ;
: >NAME ( cfa - nfa|0)  CONTEXT
   BEGIN @ DUP WHILE
      2DUP NAME> XOR
      IF   CELL-
      ELSE SWAP DROP EXIT
      THEN
   REPEAT  SWAP DROP ;
: .ID ( nfa)   COUNT $1F AND TYPE SPACE ;
: WORDS  CR CONTEXT  0 tmp ! 
   BEGIN  @ ?DUP  WHILE
      DUP SPACE .ID CELL- 
      tmp @ $0A < IF  1 tmp +!  ELSE  CR 0 tmp  !  THEN
   REPEAT ;
: FORGET  TOKEN NAME? ?DUP
   IF  CELL- DUP CP ! @ DUP CONTEXT ! LAST ! DROP EXIT  THEN
   ERROR ; RECOVER
: COLD  CR ." eForth in C, Ver 2.3,2017 " CR QUIT ; RECOVER


( Structure Compiler ========================================= )

: THEN  HERE SWAP ! ; IMMEDIATE
: FOR ( n)   COMPILE >R HERE ; IMMEDIATE
: BEGIN  HERE ; IMMEDIATE
: NEXT  COMPILE DONXT , ; IMMEDIATE
: UNTIL ( f)   COMPILE QBRANCH , ; IMMEDIATE
: AGAIN  COMPILE BRANCH , ; IMMEDIATE
: IF ( f)   COMPILE QBRANCH HERE 0 , ; IMMEDIATE
: AHEAD  COMPILE BRANCH HERE 0 , ; IMMEDIATE
: REPEAT  [COMPILE] AGAIN [COMPILE] THEN ; IMMEDIATE
: AFT  DROP AHEAD HERE SWAP ; IMMEDIATE
: ELSE  AHEAD SWAP [COMPILE] THEN ; IMMEDIATE
: WHEN  [COMPILE] IF OVER ; IMMEDIATE
: WHILE ( f)   [COMPILE] IF SWAP ; IMMEDIATE
: ABORT" ( <text>")   ['] abort" HERE ! $," ; IMMEDIATE
: $" ( <text>")   ['] $"| HERE ! $," ; IMMEDIATE
: ." ( <text>")   ['] ."| HERE ! $," ; IMMEDIATE
: CODE  TOKEN $,n OVERT ;
: CREATE ( <name>)   CODE $203D , ; ( dovar, )
: VARIABLE ( <name>)   CREATE 0 , ;
: CONSTANT ( n <name>)   CODE $2004 , , ; ( docon, )
: .( ( <text>)  $29 PARSE TYPE ; IMMEDIATE
: \  $0A WORD DROP ; IMMEDIATE
: (  $29 PARSE 2DROP ; IMMEDIATE
: COMPILE-ONLY  $40 LAST @ +! ;
: IMMEDIATE  $80 LAST @ +! ;

HERE CONSTANT end


( Boot Up ==================================================== )

0 ORG
dolist, ' COLD ,

$80 4 CELLS +
            $100 , ( 'TIB)
             $10 , ( BASE)
' IMMEDIATE 12 - , ( CONTEXT: nfa of IMMEDIATE)
             end , ( CP)
' IMMEDIATE 12 - , ( LAST: nfa of IMMEDIATE)
    ' $INTERPRET , ( 'EVAL: cfa of $INTERPRET)
          ' QUIT , ( 'ABORT: cfa of QUIT)
               0 , ( tmp)
