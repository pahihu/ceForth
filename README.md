ceForth V3.3
============

**Note**: This is C.H.Ting's [ceForth V3.3](http://www.forth.org/OffeteStore/2177-ceForth_33_macroAssemblerVS2019.zip) All modifications and bugs introduced are mine.

pahihu 7 / 4 / 2020


Changes
=======

* fixed >NAME, changed dm+ and DUMP to adapt to BytesPerWord
* added SEE
* added polled terminal I/O (curterm.h), nuf?
* added MS, as_sys replaces: as_qrx/as_txsto/as_sys
* fixed CONTEXT/LAST initialization, 64bit ceForth works
* added as_clit, rewrote system variables with as_clit, 
* instead of as_docon, 16bit ceForth works
* the C engine can be compiled as C functions, switch-threaded code, using GCC' extension `goto *expr`
* added BytesPerWord to parametrize word width (2, 4, 8)
* compilable under UNIX

