
=================
  NUMBERS PATCH
=================

Copyright (c) 2006-07, Asko Kauppi <akauppi@gmail.com>

The "Numbers" patch, also known as "lnum" or "integer patch" allows Lua
5.1 internal number mode to be easily set to a combination of any of the 
following:

General number type ('lua_Number'):

  LNUM_DOUBLE (default)         double precision FP (52 bit mantissa)
  LNUM_FLOAT                    single precision FP (23 bit mantissa)
  LNUM_LDOUBLE                  extra precision FP (80+ bit mantissa, varies)

Integer number type ('lua_Integer'):

  LNUM_INT32                    -2^31..2^31-1 integer optimized
  LNUM_INT64                    -2^63..2^63-1 integer optimized
  (none)                        all numbers stored as FP

Complex number support:

  LNUM_COMPLEX                  Complex (a+bi) number mode, scalars may be 
                                integer optimized

One shall combine these defines, i.e. '-DLNUM_DOUBLE -DLNUM_INT64' gives
normal (double) Lua range, added with full signed 64-bit integer accuracy.

With complex numbers, the general number type gives type for both real and
imaginary components. Integer number type (if any) will be applied for real
component (scalar integers).

Even though the optimized (bit accurate) integer range is signed, using 
hexadecimal notation for the values, they will behave as an unsigned 0 .. 
0xffffffff or 0xffffffffffffffff range. Care has to be applied, though, and no
arithmetic applied on the values; they are stored as signed values internally.

  Recommendations:
   - x86 and PowerPC desktops: nothing or LNUM_INT64
   - Embedded without an FPU: LNUM_INT32

  Limitations:
   - Precompiled bytecodes cannot be read into a Lua core with a different 
     number format.

Using the patch does NOT change Lua external behaviour in any way, only 
numerical accuracy is enhanced. On non-FPU platforms use of the integer
optimized modes will yield a runtime boost of 30-500% depending largely on the
application in question (for loops will see most benefits). On FPU platforms, 
use of the integer optimized modes is within +-5% of regular runtime performance.
See attached performance results for real world results.

The most external changes done by this patch are:

- integer constant VM instruction

It could be run without this, but feeding in integer constants would be risky,
and surprising loss of accuracy could occur. This allows i.e. 64-bit integer
constants.

- '_LNUM' global that carries the number mode as a string:
    "[complex] double|float|ldouble [int32|int64]"
    
This is mostly for testing purposes, normally an application would not care
to check what the number mode is (or, it can be checked as numerical expressions,
but that is a bit uncertain and complex).

The same string is also appeneded to the Lua "opening screen" printout,
alongside version, copyright etc.

- 'lua_isinteger()' addition to the API

To accompany 'lua_tointeger()' already in the API.

- 'lua_tocomplex()', 'lua_iscomplex()' and 'luaL_checkcomplex()' consistent
with the number and integer counterparts (only available with LNUM_COMPLEX).

- Following C99 complex functions added to 'math' namespace:

    scalar= math.arg()
    scalar= math.imag(c)
    scalar= math.real(c)
    complex= math.conj(c)
    complex= math.proj(c)

Note that using LNUM_COMPLEX does _not_ break Lua API that uses 'lua_Number'. 
It remains a scalar, and thus existing Lua modules should be usable also with
Complex internal number mode (of course, as long as imaginary numbers are not
fed to such modules, which will cause an error).

The LNUM_COMPLEX code relies exclusively on the C99 <complex.h> implementation
of complex numbers. Thus, in order to use LNUM_COMPLEX, also enable C99 mode
(i.e. '--std=c99' in gcc).

Currently, use of LNUM_COMPLEX needs the use of integer optimization also
(either LNUM_INT32 or LNUM_INT64). This could be relaxed, but with the cost
of more complex code.


Use of the patch:
=================

  tar xvzf lua-5.1.2
  mv lua-5.1.2 lua-5.1.2-patched
  cd lua-5.1.2-patched/src
  patch < ../../lua512_numbers.patch
  cd ..
  make linux / make macosx / your regular Lua make spell :)

or:

  make lua-5.1.2-patched
  cd lua-5.1.2-patched && make linux / make macosx / ...

or:

  make test


Remake of the patch (for developers):
=====================================

  make diff > lua_numbers.patch


-- AKa 9-Jan-2007 (started 26.8.2006) (updated 13 Aug 2007) (updated 1-2 Oct 2007)

