/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019, 2020 (c)
 * 
 *    file: comptype.h
 *    This file is part of the Layla Shell project.
 *
 *    Layla Shell is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    Layla Shell is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Layla Shell.  If not, see <http://www.gnu.org/licenses/>.
 */    

#ifndef COMPTYPE_H
#define COMPTYPE_H

/*
 * we try to guess the compiler type at compile time, by testing for 
 * a number of compiler-defined macros (which differ by the compiler, of 
 * course). these tests are built on the information provided in the following
 * three links:
 * 
 * https://blog.kowalczyk.info/article/j/guide-to-predefined-macros-in-c-compilers-gcc-clang-msvc-etc..html
 * 
 * https://sourceforge.net/p/predef/wiki/Compilers/
 * 
 * http://nadeausoftware.com/articles/2012/10/c_c_tip_how_detect_compiler_name_and_version_using_compiler_predefined_macros
 * 
 */

#if defined __CMB__
#   define COMPILER_TYPE    "Altium MicroBlaze C"
#   define COMPILER_BUILD   __BUILD__

#elif defined __CHC__
#   define COMPILER_TYPE    "Altium C-to-Hardware"
#   define COMPILER_BUILD   __BUILD__

#elif defined __CC_ARM
#   define COMPILER_TYPE    "ARM Compiler"
#   define COMPILER_BUILD   __ARMCC_VERSION

#elif defined AZTEC_C || defined __AZTEC_C__
#   define COMPILER_TYPE    "Aztec C"
#   define COMPILER_BUILD   __VERSION

#elif defined __BORLANDC__
#   define COMPILER_TYPE    "Borland C++"
#   define COMPILER_BUILD   __BORLANDC__

#elif defined __CODEGEARC__
#   define COMPILER_TYPE    "Borland C++ 2006"
#   define COMPILER_BUILD   __CODEGEARC__

#elif defined __CC65__
#   define COMPILER_TYPE    "CC65"
#   define COMPILER_BUILD   __CC65__

#elif defined __clang__

#   if defined __llvm__
#       define COMPILER_TYPE    "Clang/LLVM"
#   else
#       define COMPILER_TYPE    "Clang"
#   endif

#   if defined __apple_build_version__
#       define COMPILER_BUILD   __apple_build_version__
#   elif defined __ibmxl_vrm__
#       define COMPILER_BUILD   __ibmxl_vrm__
#   elif defined __clang_version__
#       define COMPILER_BUILD   __clang_version__
#   else
#       define COMPILER_BUILD    __clang_major__ ## "." ## __clang_minor__ ## "." ## __clang_patchlevel__ 
#   endif

#elif defined __COMO__
#   define COMPILER_TYPE    "Comeau C++"
#   define COMPILER_BUILD   __COMO_VERSION__

#elif defined __DECC
#   define COMPILER_TYPE    "Compaq C Compiler"
#   define COMPILER_BUILD   __DECC_VER

#elif defined __DECCXX
#   define COMPILER_TYPE    "Compaq C++ Compiler"
#   define COMPILER_BUILD   __DECCXX_VER

#elif defined _CRAYC
#   define COMPILER_TYPE    "Cray C"
#   define COMPILER_BUILD   _RELEASE ## "." ## _RELEASE_MINOR

#elif defined __DCC__
#   define COMPILER_TYPE    "Diab C/C++"
#   define COMPILER_BUILD   __VERSION_NUMBER__

#elif defined __DMC__
#   define COMPILER_TYPE    "Digital Mars"
#   define COMPILER_BUILD   __DMC__

#elif defined __SYSC__
#   define COMPILER_TYPE    "Dignus Systems/C++"
#   define COMPILER_BUILD   __SYSC_VER__

#elif defined __DJGPP__
#   define COMPILER_TYPE    "DJGPP"
#   define COMPILER_BUILD   __DJGPP__ ## "." ## __DJGPP_MINOR__

#elif defined __EDG__
#   define COMPILER_TYPE    "EDG C++ Frontend"
#   define COMPILER_BUILD   __EDG_VERSION__

#elif defined __PATHCC__
#   define COMPILER_TYPE    "EKOPath"
#   define COMPILER_BUILD   __PATHCC__ ## "." ## __PATHCC_MINOR__ ## "." ## __PATHCC_PATCHLEVEL__

#elif (defined __GNUC__ || defined __GNUG__) && !(defined __clang__ || defined __INTEL_COMPILER)
#   define COMPILER_TYPE    "GCC C/C++"
#   if defined __VERSION__
#       define COMPILER_BUILD   __VERSION__
#   else
#       define COMPILER_BUILD   __GNUC__ ## "." ## __GNUC_MINOR__ ## "." ## __GNUC_PATCHLEVEL__
#   endif

#elif defined __ghs__
#   define COMPILER_TYPE    "Green Hill C/C++"
#   define COMPILER_BUILD   __GHS_VERSION_NUMBER__

#elif defined __HP_aCC || defined __HP_cc
#   define COMPILER_TYPE    "HP C/aC++"
#   define COMPILER_BUILD   __HP_aCC

#elif defined __IAR_SYSTEMS_ICC__
#   define COMPILER_TYPE    "IAR C/C++"
#   define COMPILER_BUILD   __VER__

#elif defined __IBMC__ || defined __IBMCPP__
#   if defined __COMPILER_VER__
#      define COMPILER_TYPE    "IBM z/OS XL C/C++ Compiler"
#      define COMPILER_BUILD   __COMPILER_VER__
#   else
#      define COMPILER_TYPE    "IBM XL C/C++ Compiler (legacy)"
#      define COMPILER_BUILD   __IBMC__
#   endif

#elif defined __INTEL_COMPILER || defined __ICC
#   define COMPILER_TYPE    "Intel C/C++"
#   define COMPILER_BUILD   __INTEL_COMPILER

#elif defined __KCC
#   define COMPILER_TYPE    "KAI C++"
#   define COMPILER_BUILD   __KCC_VERSION

#elif defined __CA__ || defined __KEIL__
#   define COMPILER_TYPE    "KEIL CARM"
#   define COMPILER_BUILD   __CA__

#elif defined __C166__
#   define COMPILER_TYPE    "KEIL C166"
#   define COMPILER_BUILD   __C166__

#elif defined __C51__ || defined __CX51__
#   define COMPILER_TYPE    "KEIL C51"
#   define COMPILER_BUILD   __C51__

#elif defined __MWERKS__
#   define COMPILER_TYPE    "Metrowerks CodeWarrior"
#   define COMPILER_BUILD   __MWERKS__

#elif defined __CWCC__
#   define COMPILER_TYPE    "Metrowerks CodeWarrior"
#   define COMPILER_BUILD   __CWCC__

#elif defined _MSC_VER
#   define COMPILER_TYPE    "Microsoft Visual C++"
#   define COMPILER_BUILD   _MSC_VER ## " (" ## _MSC_FULL_VER ## ")"

#elif defined __MINGW64__
#   define COMPILER_TYPE    "MinGW-w64 64bit"
#   define COMPILER_BUILD   __MINGW64_VERSION_MAJOR ## "." ## __MINGW64_VERSION_MINOR

#elif defined __MINGW32__
#   define COMPILER_TYPE    "MinGW/MinGW-w64 32bit"
#   define COMPILER_BUILD   __MINGW32_MAJOR_VERSION ## "." ## __MINGW32_MINOR_VERSION

#elif defined __sgi || defined sgi
#   define COMPILER_TYPE    "MIPSpro"
#   if defined _COMPILER_VERSION
#       define COMPILER_BUILD   _COMPILER_VERSION
#   else
#       define COMPILER_BUILD   _SGI_COMPILER_VERSION
#   endif

#elif defined __MRC__
#   define COMPILER_TYPE    "MPW C++"
#   define COMPILER_BUILD   __MRC__

#elif defined __CC_NORCROFT
#   define COMPILER_TYPE    "Norcroft C"
#   define COMPILER_BUILD   __ARMCC_VERSION

#elif defined __OPEN64__
#   define COMPILER_TYPE    "Open64"
#   define COMPILER_BUILD   __OPEN64__

#elif defined __OPENCC__
#   define COMPILER_TYPE    "Open64"
#   define COMPILER_BUILD   __OPENCC__ ## "." ## __OPENCC_MINOR__ ## "." ## __OPENCC_PATCHLEVEL__

#elif defined __SUNPRO_C
#   define COMPILER_TYPE    "Oracle Solaris Studio C Compiler"
#   define COMPILER_BUILD   __SUNPRO_C

#elif defined __SUNPRO_CC
#   define COMPILER_TYPE    "Oracle Solaris Studio C++ Compiler"
#   define COMPILER_BUILD   __SUNPRO_CC

#elif defined _PACC_VER
#   define COMPILER_TYPE    "Palm C/C++"
#   define COMPILER_BUILD   _PACC_VER

#elif defined __POCC__
#   define COMPILER_TYPE    "Pelles C"
#   define COMPILER_BUILD   __POCC__

#elif defined __PGI
#   define COMPILER_TYPE    "Portland Group C/C++"
#   define COMPILER_BUILD   __PGIC__ ## "." ## __PGIC_MINOR__ ## "." ## __PGIC_PATCHLEVEL__

#elif defined __RENESAS__
#   define COMPILER_TYPE    "Renesas C/C++"
#   define COMPILER_BUILD   __RENESAS_VERSION__

#elif defined __HITACHI__
#   define COMPILER_TYPE    "Renesas C/C++"
#   define COMPILER_BUILD   __HITACHI_VERSION__

#elif defined SASC || defined __SASC || defined __SASC__
#   define COMPILER_TYPE    "SAS/C"
#   if defined __SASC__
#       define COMPILER_BUILD   __SASC__
#   else
#       define COMPILER_BUILD   __VERSION__ ## "." ## __REVISION__
#   endif

#elif defined SDCC
#   define COMPILER_TYPE    "Small Device C Compiler"
#   define COMPILER_BUILD   SDCC

#elif defined __VOSC__
#   define COMPILER_TYPE    "Stratus VOS C"
#   define COMPILER_BUILD   __VOSC__

#elif defined __SC__
#   define COMPILER_TYPE    "Symantec C++"
#   define COMPILER_BUILD   __SC__

#elif defined __TI_COMPILER_VERSION__
#   define COMPILER_TYPE    "Texas Instruments C/C++ Compiler"
#   define COMPILER_BUILD   __TI_COMPILER_VERSION__

#elif defined THINKC3
#   define COMPILER_TYPE    "THINK C"
#   define COMPILER_BUILD   "3.x"

#elif defined THINKC4
#   define COMPILER_TYPE    "THINK C"
#   define COMPILER_BUILD   "4.x"

#elif defined __TURBOC__
#   define COMPILER_TYPE    "Turbo C/C++"
#   define COMPILER_BUILD   __TURBOC__

#elif defined _UCC
#   define COMPILER_TYPE    "Ultimate C/C++"
#   define COMPILER_BUILD   _MAJOR_REV ## "." ## _MINOR_REV

#elif defined __USLC__
#   define COMPILER_TYPE    "USL C"
#   define COMPILER_BUILD   __SCO_VERSION__

#elif defined __WATCOMC__
#   define COMPILER_TYPE    "Watcom C++"
#   define COMPILER_BUILD   __WATCOMC__

#elif defined __ZTC__
#   define COMPILER_TYPE    "Zortech C++"
#   define COMPILER_BUILD   __ZTC__

#else
#   define COMPILER_TYPE    "Unknown"
#   define COMPILER_BUILD   "Unknown"

#endif

#endif      /* COMPTYPE_H */
