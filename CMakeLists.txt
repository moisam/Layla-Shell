#  
#     Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
#     Copyright 2016, 2017, 2018, 2019 (c)
#  
#     file: CMakeLists.txt
#     This file is part of the Layla Shell project.
# 
#     Layla Shell is free software: you can redistribute it and/or modify
#     it under the terms of the GNU General Public License as published by
#     the Free Software Foundation, either version 3 of the License, or
#     (at your option) any later version.
# 
#     Layla Shell is distributed in the hope that it will be useful,
#     but WITHOUT ANY WARRANTY; without even the implied warranty of
#     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#     GNU General Public License for more details.
# 
#     You should have received a copy of the GNU General Public License
#     along with Layla Shell.  If not, see <http://www.gnu.org/licenses/>.
#      

# if you are using Eclipse, you can build the project files from the source
# directory by running:
#
#    $ cmake -G "Eclipse CDT4 - Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_ECLIPSE_MAKE_ARGUMENTS=-j8 .
#
# then open Eclipse and select File -> Import... -> General -> Existing Projects into Workspace
# and select the source directory.

cmake_minimum_required(VERSION 2.6)
project(shell)

# we set the debug mode so that we can easily use gdb and valgrind later.
# don't forget to turn off in the release version!

set(CMAKE_BUILD_TYPE Debug)

# gcc flags

set(CMAKE_C_FLAGS "-Wall -Wextra")

# the source files

add_executable(lsh   main.c wordexp.c       cmdline.c               helpfunc.c   initsh.c
                     jobs.c prompt.c        args.c                  params.c  sig.c tab.c
                     debug.c                kbdevent2.c             shunt.c      alphalist.c
                     early_environ.h        cmd.h                   sig.h   braceexp.c
                     debug.h                kbdevent.h              cpu.h     ostype.h   vars.c
                     vi.c vi.h vi_keys.c    comptype.h  strbuf.c    popen.c   functab.c
                     scanner/lexical.c      scanner/source.c
                     scanner/scanner.h      scanner/source.h        scanner/keywords.h
                     parser/node.c          parser/parser.c         parser/conditionals.c
                     parser/node.h          parser/loops.c          parser/redirect.c
                     backend/backend.c      backend/backend.h       backend/pattern.c
                     backend/redirect.c     backend/conditionals.c  backend/loops.c
                     symtab/symtab_hash.c   symtab/symtab_hash.h    symtab/symtab.h
                     symtab/string_hash.c   symtab/string_hash.h
                     error/error.c          error/error.h
                     builtins/builtins.c
                     builtins/help.c        builtins/ulimit.c       builtins/source.c
                     builtins/history.c     builtins/hash.c         builtins/type.c
                     builtins/alias.c       builtins/bg.c           builtins/cd.c
                     builtins/command.c     builtins/false.c        builtins/fc.c
                     builtins/fg.c          builtins/getopts.c      builtins/kill.c
                     builtins/newgrp.c      builtins/pwd.c          builtins/read.c
                     builtins/true.c        builtins/umask.c        builtins/unalias.c
                     builtins/wait.c        builtins/time.c         builtins/mailcheck.c
                     builtins/colon.c       builtins/dot.c          builtins/eval.c
                     builtins/exec.c        builtins/exit.c         builtins/export.c
                     builtins/readonly.c    builtins/return.c       builtins/set.c
                     builtins/shift.c       builtins/times.c        builtins/trap.c
                     builtins/unset.c       builtins/ver.c          builtins/dump.c
                     builtins/test.c        builtins/disown.c       builtins/echo.c
                     builtins/let.c         builtins/whence.c       builtins/local.c
                     builtins/setx.c        builtins/setx.h         builtins/coproc.c
                     builtins/caller.c      builtins/declare.c      builtins/enable.c
                     builtins/logout.c      builtins/memusage.c     builtins/dirstack.c
                     builtins/suspend.c     builtins/hist_expand.c  builtins/bugreport.c
                     builtins/nice.c        builtins/hup.c          builtins/notify.c
                     builtins/glob.c        builtins/printenv.c     builtins/repeat.c
                     builtins/setenv.c      builtins/stop.c         builtins/unlimit.c
                     builtins/unsetenv.c
                     )
                     
# librt is needed for timer_create() and timer_settime()
target_link_libraries (lsh rt)

# compile and install

install(TARGETS lsh RUNTIME DESTINATION bin)
