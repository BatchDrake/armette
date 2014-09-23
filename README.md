ARMette
=======

ARMette is a standalone user-space ARM7 emulation C library designed to work as a support library for ARM reverse engineers. In this early version it only supports ARMel ELF32 loading under little endian platforms, emulated function calling and import overriding facilities. The full instruction set is not implemented yet. However, some features I'd like to write are:

* Full ARM7 instruction set support
* Thumb instructions
* Speed optimizations (the most complex data structure you'll find in the code is a pointer list, segment translations should be implemented with trees)
* Breakpoints / watchpoints
* Smarter debug output
* Direct function call conversion

I don't think it would make sense to implement actual syscalls as QEMU does it already and I don't intend to replace it.

Compilation
-----------
To compile ARMette from github you will need to install autoconf, automake and libtool (usually found in the software repositories of all major GNU/Linux distributions). And git, of course ;)

First, you need to clone ARMette:

```
% git clone http://github.com/BatchDrake/armette
% cd armette
```
And then, once in the armette directory:

```
% autoreconf -fvi
% ./configure
% make
% sudo make install
```

And if you found no errors, voilà! ARMette is installed in your system.

Usage
-----
To compile software with ARMette, you will need to install pkg-config. As a proof of concept, I provide a one-file emulator to work on top of ARMette (see below)

This file, called emulator.c, can be compiled just by running:

```
% gcc emulator.c -o emulator `pkg-config armette --cflags --libs`
```
Right now, I implemented a few hooks only (the required to make a binary like GNU coreutil's cat work). Most functions are unimplemented yet, but grab a copy of cat for ARM and check it. It's very likely that any other binary will fail with a undefined function error.

```
% ./emulator cat /etc/motd
% ./emulator cat --help
% ./emulator cat
```

emulator.c
----------
This is one of the simplest use cases of ARMette, a small emulator that takes an ARM binary, builds its environment and runs it.

```cpp
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <armette.h>

int
main (int argc, char **argv)
{
  struct arm32_cpu *cpu;
  int ret;
  
  if (argc < 2)
  {
    fprintf (stderr, "%s: insufficient number of arguments\n", argv[0]);
    
    fprintf (stderr, "Usage:\n\t%s binary [arguments...]\n", argv[0]);

    exit (EXIT_FAILURE);
  }

  if ((cpu = arm32_cpu_new_from_elf (argv[1])) == NULL)
  {
    fprintf (stderr, "%s: cannot load %s: %s\n", argv[0], argv[1], strerror (errno));

    exit (EXIT_FAILURE);
  }

  /* Hook .plt imports and redirect them to native implementations */
  arm32_init_stdlib_hooks (cpu);

  /* Prepare environment for main () in order to start execution */
  if (arm32_cpu_prepare_main (cpu, argc - 1, argv + 1) == -1)
  {
    fprintf (stderr, "%s: cannot prepare main context for %s: %s\n", argv[0], argv[1], strerror (errno));

    arm32_cpu_destroy (cpu);

    exit (EXIT_FAILURE);
  }

  /* Run normally */
  if ((ret = arm32_cpu_run (cpu)) != 0)
    fprintf (stderr, "%s: exception raised by %s: #%d\n", argv[0], argv[1], EXCODE (ret));

  arm32_cpu_destroy (cpu);
  
  return 0;
}

```
