/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019, 2020 (c)
 * 
 *    file: ostype.h
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

#ifndef OSTYPE_H
#define OSTYPE_H

/*
 * we try to guess the running Operating System at compile time, by testing for 
 * a number of compiler-defined macros (which differ by the compiler, of 
 * course). these tests are built on the information provided in the following
 * two links:
 * 
 * https://sourceforge.net/p/predef/wiki/OperatingSystems/
 * 
 * http://nadeausoftware.com/articles/2012/01/c_c_tip_how_use_compiler_predefined_macros_detect_operating_system
 * 
 */

/* AIX */
#if defined(_AIX) || defined(__TOS_AIX__)
#   define OS_TYPE     "AIX"

/* Android */
#elif defined(__ANDROID__)
#   define OS_TYPE     "Android"

/* UTS */
#elif defined(UTS)
#   define OS_TYPE     "Amdahl UTS"

/* AmigaOS */
#elif defined(AMIGA) || defined(__amigaos__)
#   define OS_TYPE     "AmigaOS"

/* Apollo AEGIS */
#elif defined(aegis)
#   define OS_TYPE     "Apollo AEGIS"

/* Apollo Domain/OS */
#elif defined(apollo)
#   define OS_TYPE     "Apollo Domain/OS"

/* BeOS */
#elif defined(__BEOS__)
#   define OS_TYPE     "BeOS"

/* Blue Gene */
#elif defined(__bg__) || defined(__bgq__)
#   define OS_TYPE     "Blue Gene"

/* Apple OS X */
#elif defined(__APPLE__) && defined(__MACH__)
#   define OS_TYPE     "Apple OS X"

/* BSD family */
#elif defined(__FreeBSD__)
#   define OS_TYPE     "FreeBSD"

#elif defined(__NetBSD__)
#   define OS_TYPE     "NetBSD"

#elif defined(__OpenBSD__)
#   define OS_TYPE     "OpenBSD"

#elif defined(__DragonFly__)
#   define OS_TYPE     "DragonFly"

#elif defined(__bsdi__)
#   define OS_TYPE     "BSD"

#elif defined(__FreeBSD_kernel__)
#   if defined(__GLIBC__)
#       define OS_TYPE     "GNU/kFreeBSD"
#   else
#       define OS_TYPE     "FreeBSD"
#   endif

/* ConvexOS */
#elif defined(__convex__)
#   define OS_TYPE     "ConvexOS"

/* Cygwin environment */
#elif defined(__CYGWIN__)
#   define OS_TYPE     "Cygwin"

/* DG/UX */
#elif defined(DGUX) || defined(__DGUX__) || defined(__dgux__)
#   define OS_TYPE     "DG/UX"

/* DYNIX/ptx */
#elif defined(_SEQUENT_) || defined(sequent)
#   define OS_TYPE     "DYNIX/ptx"

/* ECos */
#elif defined(__ECOS)
#   define OS_TYPE     "ECos"

/* EMX environment */
#elif defined(__EMX__)
#   define OS_TYPE     "EMX"

/* GNU/Hurd */
#elif defined(__gnu_hurd__) || defined(__GNU__)
#   define OS_TYPE     "GNU/Hurd"

/* GNU/Linux */
#elif defined(__gnu_linux__)
#   define OS_TYPE     "GNU/Linux"

/* HI-UX MPP */
#elif defined(__hiuxmpp)
#   define OS_TYPE     "HI-UX MPP"

/* HP-UX */
#elif defined(_hpux) || defined(hpux) || defined(__hpux)
#   define OS_TYPE     "HP-UX"

/* IBM OS/400 */
#elif defined(__OS400__)
#   define OS_TYPE     "IBM OS/400"

/* Integrity */
#elif defined(__INTEGRITY)
#   define OS_TYPE     "Integrity"

/* Irix */
#elif defined(sgi) || defined(__sgi)
#   define OS_TYPE     "Irix"

/* LynxOS */
#elif defined(__Lynx__)
#   define OS_TYPE     "LynxOS"

/* Mac OS 9 */
#elif defined(macintosh) || defined(Macintosh)
#   define OS_TYPE     "Mac OS 9"

/* MINIX */
#elif defined(__minix)
#   define OS_TYPE     "MINIX"

/* MorphOS */
#elif defined(__MORPHOS__)
#   define OS_TYPE     "MorphOS"

/* MS DOS */
#elif defined(MSDOS) || defined(__MSDOS__) || defined(_MSDOS) || defined(__DOS__)
#   define OS_TYPE     "MS DOS"

/* Nucleus RTOS */
#elif defined(__nucleus__)
#   define OS_TYPE     "Nucleus RTOS"

/* OS/2 */
#elif defined(OS2) || defined(_OS2) || defined(__OS2__) || defined(__TOS_OS2__)
#   define OS_TYPE     "OS/2"

/* PalmOS */
#elif defined(__palmos__)
#   define OS_TYPE     "PalmOS"

/* Plan 9 */
#elif defined(EPLAN9)
#   define OS_TYPE     "Plan 9"

/* QNX */
#elif defined(__QNX__) || defined(__QNXNTO__)
#   define OS_TYPE     "QNX"

/* SCO OpenServer */
#elif defined(M_I386) || defined(M_XENIX) || defined(_SCO_DS)
#   define OS_TYPE     "SCO OpenServer"

/* Solaris/SunOS */
#elif defined(sun) || defined(__sun)
#   if defined(__SVR4) || defined(__svr4__)
#       define OS_TYPE     "Solaris"
#   else
#       define OS_TYPE     "SunOS"
#   endif

/* Stratus VOS */
#elif defined(__VOS__)
#   define OS_TYPE     "Stratus VOS"

/* Syllable */
#elif defined(__SYLLABLE__)
#   define OS_TYPE     "Syllable"

/* Symbian OS */
#elif defined(__SYMBIAN32__)
#   define OS_TYPE     "Symbian OS"

/* OSF/1 */
#elif defined(__osf__) || defined(__osf)
#   define OS_TYPE     "OSF/1"

/* Ultrix */
#elif defined(ultrix) || defined(__ultrix) || defined(__ultrix__)
#   define OS_TYPE     "Ultrix"

/* UNICOS */
#elif defined(_UNICOS)
#   define OS_TYPE     "UNICOS"

/* UnixWare */
#elif defined(sco) || defined(_UNIXWARE7)
#   define OS_TYPE     "UnixWare"

/* VMS */
#elif defined(VMS) || defined(__VMS)
#   define OS_TYPE     "VMS"

/* VxWorks */
#elif defined(__VXWORKS__) || defined(__vxworks)
#   define OS_TYPE     "VxWorks"

/* Windows */
#elif defined(_WIN16) || defined(_WIN32) || defined(_WIN64) || defined(__WIN32__) || defined(__TOS_WIN__) || defined(__WINDOWS__)
#   define OS_TYPE     "Windows"

/* Windows CE */
#elif defined(_WIN32_WCE)
#   define OS_TYPE     "Windows CE"

/* z/OS */
#elif defined(__MVS__) || defined(__HOS_MVS__) || defined(__TOS_MVS__)
#   define OS_TYPE     "z/OS"

/* Linux kernel */
#elif defined(__linux__)
#   define OS_TYPE     "Linux"

/* Unix SVR4 environment */
#elif defined(__sysv__) || defined(__SVR4) || defined(__svr4__)
#   define OS_TYPE     "Unix SVR4"

#elif defined(__unix__)
#include <sys/param.h>
#   if defined(BSD)
#       define OS_TYPE     "BSD"
#   else
#       define OS_TYPE     "Unix"
#   endif

/* if nothing identified, set OS type to unknown */
#else
#   define OS_TYPE     "Unknown"
#endif


#endif
