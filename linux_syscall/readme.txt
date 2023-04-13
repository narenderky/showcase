	This directory contains a set of userspace and kernel space programs to demostrate how one can implement their own system call in Linux kernel. Not that kernel programmers need to add, remove or modify system call every day, but going through this exercise also improves ones understanding about how system calls work in general.

The sub-directory userspace contains a very simple way to call our newly added system call using its system call number. While the sub-directory kernel_src contains the actual implementation of the system call and the modifications required to the kernel source code.

Files modified:
kernel_src/Makefile - This is the top level Linux kernel Makefile. Modified this file to make the build system aware of the location of our source code.
kernel_src/include/linux/syscalls.h - Added a prototype entry for our syscall in this file.
./kernel_src/arch/x86/syscalls/syscall_64.tblf - Added our system call to the system call table.
