( Utility words)

: nuf? ( - f)   ?KEY DUP IF  2DROP KEY 0D =  THEN ;
: SEE ( <name>)   ' CR CELL+
   BEGIN
      1+ DUP @ DUP
      IF >NAME THEN
      ?DUP IF   SPACE .ID 1- CELL+
           ELSE DUP C@ U.
           THEN
      100 MS  nuf?
   UNTIL  DROP ;

: RECURSE   LAST @ NAME> , ; IMMEDIATE COMPILE-ONLY
