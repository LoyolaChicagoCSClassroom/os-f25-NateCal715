

# i386 OS


This is a shell for a custom bare metal operating system on the Intel 386. It is intended to be run inside `qemu` on the i386 target.

## Tools

You first need to upgrade all your system's software:

```
user@system:~ $ sudo apt update && sudo apt upgrade
```


```
user@system:~ $ sudo apt install qemu-system-i386 vim screen gcc-i686-linux-gnu gdb-multiarch make git mtools grub-pc
```


## ARM Laptop Users: Build Custom GRUB

```
user@system ~ $ sudo apt install autoconf gettext autopoint pkg-config flex bison
user@system ~ $ git clone https://git.savannah.gnu.org/git/grub.git
user@system ~ $ cd grub
user@system ~ $ ./bootstrap
user@system ~ $ ./configure --with-platform=pc --target=i686-linux-gnu --prefix=/usr/local/grub/
user@system ~ $ make
user@system ~ $ sudo make install
```



## Features of the Makefile

The Makefile in this repo has a bunch of useful recipes that you can use:

1. `make` or `make bin` builds the kernel binary `kernel8.img` along with `kernel8.elf`. Both are binary files that contain the compiled code of our operating system. The difference is that `kernel8.img` can be loaded by the Pi bootloader, and `kernel8.elf` is in a standard format that is recognized by tools like `gdb`.
2. `make disassemble | less` disassembles the kernel binary. Useful if you need to see where functions or variables are located in memory.
3. `make debug` runs the kernel in qemu while allowing you to step through it line-by-line in gdb.
4. `make run` runs your kernel in qemu with no debugger.
5. `make clean` removes all compiled object files.

## Adding to the Shell Code

The best way to add features is to create a new source file in the `src` directory. If you create a new source file, you will need to add it to the `OBJS` list in the Makefile (starting around line 15). For example, say you create a new file called `src/neil.c`. You will need add a new line in the Makefile:


```
OBJS = \
    kernel_main.o \
    neil.o \

```

Note that the new line we added to the `OBJS` list was `neil.o`, not `neil.c`. Also, you need to make sure you have an empty line after the last element of the `OBJS` list, otherwise `make` will complain.


# linked-lists-NateCal715
