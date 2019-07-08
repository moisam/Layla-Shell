# About
Layla shell is a POSIX-compliant GNU/Linux shell. It aims to implement the
full functionality specified in the POSIX Shell & Utilities volume. This
volume describes the Shell Command Language, the general terminal interface,
and the builtin utilities of compliant shells. The full POSIX standard is
freely available from The Open Group's website at:

https://pubs.opengroup.org/onlinepubs/9699919799.2018edition/

Layla shell implements most of the POSIX-defined functionality. Specifically,
quoting, token recognition, words expansions, I/O redirection, reponses to
error conditions, the shell command language, the standard shell execution
environment, signal handling, and the general and special builtin utilities
are all implemented and behave in a way that is conforming to the POSIX
standard. Pattern matching and pathname expansion relies on external tools,
and thus might not behave exactly as specified in the POSIX standard. The
shell grammar has been extended to accommodate some of the widely used non-
POSIX words, such as the `function` and `[[` keywords and the `(( ))` arithmetic
expansion operator. In all of these situations, we followed ksh behaviour,
as ksh is the "model" POSIX shell, the one upon which the Shell & Utilities
volume was modeled in the first place. But ksh is not the only shell out
there. This is why we included features from other shells, most notably bash,
the most widely used shell nowadays.

To better understand the inner workings of this (as well as any other POSIX-
compliant shell), it would be better if you got a copy of the Linux Shell
Internals book, soon to be published by No Starch Press. This shell and the book has been
developed hand-in-hand. The book helps to explain the code of this shell and
provide a walk-through for newcomers. Although the code is extensively
commented, a lot of theoretical ground has covered in the book, not in the
source code. If you really want to understand how and why Unix/Linux shells
behave in the context of POSIX, I honestly advise you to get a copy of the
book.

There is still a long way to go with testing, bug-fixing and improving Layla
shell, and your feedback is more than welcome in this regard. If you have a
bug report, or you want to suggest adding some feature or fixing something
with the shell, feel free to email me directly at my email address, the one
you will find in the beginning of this file. If you have any bugfixes, patches
or feature improvements you want to add to the code, feel free to send me your
code at the email address above, or through the Linux Shell Internals
repository at GitHub:

https://github.com/moisam/Linux-Shell-Internals-book/


# Package dependencies
Layla shell has very few external dependencies, in order to ease the process
of compiling and installing it. You only need to have the GNU C library
installed, in addition to the GNU Compiler Collection (GCC) or the Clang/LLVM
compiler.

If you are using any GNU/Linux distro, it would be better if you checked your
distro's official repositories, as these tools are installed by default on
most systems. If, by a sore chance, you needed to manually download and
install them, here are the links:

* GNU C Library: https://www.gnu.org/software/libc/
* GCC: https://gcc.gnu.org/
* LLVM: http://releases.llvm.org/download.html


# How to compile and install
If you downloaded the shell as a source tarball, navigate to the directory
where you downloaded the tarball:

```
$ cd ~/Downloads
```

then extract the archive and enter the extracted directory:

```
$ tar xvf lsh-1.0
$ cd lsh-1.0/
```

then run:

```
$ make && make install
```

and that's it! Now you can run the shell by invoking:

```
$ lsh
```

# Help
For more information, please read the manpage and info page in the `docs/`
directory.

Thanks!
