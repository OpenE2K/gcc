// Copyright (C) 1994-2017 Free Software Foundation, Inc.
//
// This file is part of GCC.
//
// GCC is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3, or (at your option)
// any later version.

// GCC is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// Under Section 7 of GPL version 3, you are granted additional
// permissions described in the GCC Runtime Library Exception, version
// 3.1, as published by the Free Software Foundation.

// You should have received a copy of the GNU General Public License and
// a copy of the GCC Runtime Library Exception along with this program;
// see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
// <http://www.gnu.org/licenses/>.

#include "tinfo.h"

namespace __cxxabiv1 {

// This has special meaning to the compiler, and will cause it
// to emit the type_info structures for the fundamental types which are
// mandated to exist in the runtime.
__fundamental_type_info::
~__fundamental_type_info ()
{}

}

#ifdef __LCC__

namespace std
{

/*
Define the type_info objects that are supposed to be present in the runtime
library.  Try to work hard enough that the compiler won't eliminate the
generation of the type_info objects.
*/
const type_info *__dummy_typeinfo;
void __gen_dummy_typeinfos()
{
#define gen_typeinfos(type)                 \
  __dummy_typeinfo = &typeid(type),         \
  __dummy_typeinfo = &typeid(type *),       \
  __dummy_typeinfo = &typeid(const type *)
  gen_typeinfos(void); 
  gen_typeinfos(bool); 
  gen_typeinfos(wchar_t);
  gen_typeinfos(char16_t);
  gen_typeinfos(char32_t);
  gen_typeinfos(char); 
  gen_typeinfos(signed char); gen_typeinfos(unsigned char); 
  gen_typeinfos(short);       gen_typeinfos(unsigned short); 
  gen_typeinfos(int);         gen_typeinfos(unsigned int); 
  gen_typeinfos(long);        gen_typeinfos(unsigned long); 
  gen_typeinfos(long long);   gen_typeinfos(unsigned long long);
  gen_typeinfos(float); 
  gen_typeinfos(double); 
  gen_typeinfos(long double);
#if __EDG_CPP11_IL_EXTENSIONS_SUPPORTED
  gen_typeinfos(decltype(nullptr));
#endif /* if __EDG_CPP11_IL_EXTENSIONS_SUPPORTED */
#if __EDG_FLOAT80_ENABLING_POSSIBLE
  gen_typeinfos(__float80);
#endif /* if __EDG_FLOAT80_ENABLING_POSSIBLE */
#if __EDG_FLOAT128_ENABLING_POSSIBLE
  gen_typeinfos(__float128);
#endif /* if __EDG_FLOAT128_ENABLING_POSSIBLE */
#if __EDG_FLOAT128_ENABLING_POSSIBLE
  gen_typeinfos(__float128);
#endif /* if __EDG_FLOAT128_ENABLING_POSSIBLE */
#if __EDG_INT128_EXTENSIONS_ALLOWED && \
    (defined(__GNUC__) || defined(__clang__))
  /* These types are only available if the front end is running in GCC
     emulation mode. */
  gen_typeinfos(__int128_t);
  gen_typeinfos(__uint128_t);
#endif /* if __EDG_INT128_EXTENSIONS_ALLOWED && defined(__GNUC__)...*/
#undef gen_typeinfos
}

} // namespace std

#endif // __LCC__
