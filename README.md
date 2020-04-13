ceForth V3.3
============

**Note**: This is C.H.Ting's [ceForth V3.3](http://www.forth.org/OffeteStore/2177-ceForth_33_macroAssemblerVS2019.zip) All modifications and bugs introduced are mine. I am sure, this will NOT work on big-endian machines.

Change `BytesPerWord` to 2/4/8 to define the word width. `#define STC` to use switch-threaded interpreter, add `#define GCC_DISPATCH` to use GCC's goto. 

Good luck !


pahihu    7 / 4 / 2020


Changes
=======

* removed douse, added >ABS >REL DLSYM CALLC, moved additions to system.f/utils.f/timer.f
* added douse
* added rpat, rpsto, spat, spsto
* embedded stack/rack
* fixed >NAME, changed dm+ and DUMP to adapt to BytesPerWord
* added SEE
* added polled terminal I/O (curterm.h), nuf?
* added MS, as_sys replaces: as_qrx/as_txsto/as_sys
* fixed CONTEXT/LAST initialization, 64bit ceForth works
* added as_clit, rewrote system variables with as_clit, instead of as_docon, 16bit ceForth works
* the C engine can be compiled as C functions, switch-threaded code, using GCC' extension `goto *expr`
* added BytesPerWord to parametrize word width (2, 4, 8)
* compilable under UNIX

