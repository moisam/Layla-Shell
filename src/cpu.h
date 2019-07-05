/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
 * 
 *    file: cpu.h
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

#ifndef CPU_H
#define CPU_H

/*
 * we try to guess the cpu architecture at compile time, by testing for 
 * a number of compiler-defined macros (which differ by the compiler, of 
 * course). these tests are built on the information provided in the following
 * two links:
 * 
 * https://sourceforge.net/p/predef/wiki/Architectures/
 * 
 * http://nadeausoftware.com/articles/2012/02/c_c_tip_how_detect_processor_type_using_compiler_predefined_macros
 * 
 */

/* DEC Alpha */
#if defined(__alpha__) || defined(__alpha) || defined(_M_ALPHA)
#   define CPU_ARCH     "DEC Alpha"

/* AMD64 */
#elif defined(__x86_64__) || defined(__amd64__) || defined(_M_X64) || defined(_M_AMD64)
#   define CPU_ARCH     "AMD64"

/* ARM */
#elif defined(__arm) || defined(__arm__) || defined(__TARGET_ARCH_ARM) || defined(_ARM) || defined(_M_ARM) || defined(_M_ARMT)
#   if defined(__aarch64__)
#       define CPU_ARCH     "ARM64"
#   else
#       define CPU_ARCH     "ARM"
#   endif

/* Blackfin */
#elif defined(__bfin) || defined(__BFIN__)
#   define CPU_ARCH     "Blackfin"

/* Convex */
#elif defined(__convex__)
#   define CPU_ARCH     "Convex"

/* Epiphany */
#elif defined(__epiphany__)
#   define CPU_ARCH     "Epiphany"

/* HP/PA RISC */
#elif defined(__hppa__) || defined(__HPPA__) || defined(__hppa)
#   define CPU_ARCH     "HP/PA RISC"

/* Intel x86 */
#elif defined(__i386) || defined(__I386) || defined(_M_IX86) || defined(_M_I86) || defined(__X86__) || defined(_X86_) || defined(__I86__)
#   define CPU_ARCH     "i386"

/* Intel Itanium (IA-64) */
#elif defined(__ia64) || defined(__ia64__) || defined(__itanium__) || defined(_M_IA64)
#   define CPU_ARCH     "Intel Itanium (IA64)"

/* Motorola 68k */
#elif defined(__m68k__) || defined(M68000) || defined(__MC68K__)
#   define CPU_ARCH     "Motorola 68k"

/* MIPS */
#elif defined(__mips__) || defined(__mips) || defined(__MIPS__)
#   define CPU_ARCH     "MIPS"

/* IBM PowerPC */
#elif defined(__powerpc__) || defined(__ppc__) || defined(__ppc) || defined(__PPC__) || defined(_M_PPC) || defined(_ARCH_PPC)
#   if defined(__powerpc64__) || defined(__ppc64__) || defined(__PPC64__) || defined(__64BIT__) || defined(_LP64) || defined(__LP64__) || defined(_ARCH_PPC64)
#       define CPU_ARCH     "IBM PowerPC (64-bit)"
#   else
#       define CPU_ARCH     "IBM PowerPC (32-bit)"
#   endif

/* Oracle SPARC */
#elif defined(__sparc) || defined(__sparc__) || defined(__sparc64__)
#   define CPU_ARCH     "Oracle SPARC"

/* Hitachi SuperH */
#elif defined(__sh__)
#   define CPU_ARCH     "Hitachi SuperH"

/* IBM z/Architecture */
#elif defined(__370__) || defined(__s390__) || defined(__s390x__) || defined(__zarch__) || defined(__SYSC_ZARCH__)
#   define CPU_ARCH     "IBM Z"

/* Texas Instruments TMS320 series */
#elif defined(_TMS320C2XX) || defined(_TMS320C5X) || defined(_TMS320C6X)
#   define CPU_ARCH     "TMS320"

/* if nothing identified, set cpu arch to unknown */
#else
#   define CPU_ARCH     "Unknown"
#endif


#endif
