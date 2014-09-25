/*
 *    ARMette: a small ARM7 multiplatform emulation library
 *    Copyright (C) 2014  Gonzalo J. Carracedo
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>
 */

#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>

#include <armette.h>

#include <arm_cpu.h>
#include <arm_inst.h>
#include <arm_elf.h>

#define ARMSYM(sname) JOIN (arm32_stdlib_, sname)

#define ARMPROTO(sname) \
  int ARMSYM (sname) (struct arm32_cpu *cpu, const char *name, void *data, uint32_t prev)

extern uint32_t  arm_errno_virt;
extern uint32_t *arm_errno;
uint32_t *arm_optind;

struct arm32_stat64
{
  uint64_t	ast_dev;
  uint8_t       __pad0[4];
  uint32_t	__st_ino;
  uint32_t	ast_mode;
  uint32_t	ast_nlink;
  uint32_t	ast_uid;
  uint32_t	ast_gid;
  uint64_t	ast_rdev;
  uint8_t       __pad3[8];
  int64_t  	ast_size;
  uint32_t	ast_blksize;
  uint64_t      ast_blocks;
  uint32_t	ast_atime;
  uint32_t	ast_atime_nsec;
  uint32_t	ast_mtime;
  uint32_t	ast_mtime_nsec;
  uint32_t	ast_ctime;
  uint32_t	ast_ctime_nsec;
  uint64_t	ast_ino;
}
__attribute__ ((packed));

ARMPROTO (bindtextdomain)
{
  char *domainname;
  char *dirname;

  if ((domainname = arm32_cpu_translate_read (cpu, R0 (cpu))) == NULL)
    EXCEPT (ARM32_EXCEPTION_DATA);

  if ((dirname = arm32_cpu_translate_read (cpu, R1 (cpu))) == NULL)
    EXCEPT (ARM32_EXCEPTION_DATA);

  debug ("Bind text domain: \"%s\" to directory \"%s\"\n", domainname, dirname);

  arm32_cpu_return (cpu);
  
  return 0;
}

ARMPROTO (setlocale)
{
  int dom;
  char *locale;

  dom = R0 (cpu);

  static uint32_t last;
  
  if (R1 (cpu) == 0)
  {
    R0 (cpu) = last;
    debug ("Return last locale at 0x%x\n", last);
  }
  else
  {
    if ((locale = arm32_cpu_translate_read (cpu, last = R1 (cpu))) == NULL)
      EXCEPT (ARM32_EXCEPTION_DATA);
    
    debug ("Set locale of domain #%d to \"%s\"\n", dom, locale);
  }
  
  arm32_cpu_return (cpu);

  return 0;
}

ARMPROTO (strrchr)
{
  char *haystack;
  char *where;
  
  if ((haystack = arm32_cpu_translate_read (cpu, R0 (cpu))) == NULL)
    EXCEPT (ARM32_EXCEPTION_DATA);
  
  debug ("Strrchr: \"%s\"\n", haystack);
  debug ("Char: '%c'\n", R1 (cpu));

  if ((where = strrchr (haystack, R1 (cpu))) == NULL)
    R0 (cpu) = 0;
  else
    R0 (cpu) = (where - haystack) + R0 (cpu);

  arm32_cpu_return (cpu);
  
  return 0;
}

ARMPROTO (fwrite)
{
  char *buf;
  int len;
  int pieces;
  uint32_t dest;
  
  if ((buf = arm32_cpu_translate_read (cpu, R0 (cpu))) == NULL)
    EXCEPT (ARM32_EXCEPTION_DATA);

  len = R1 (cpu);
  pieces = R2 (cpu);
  dest = R3 (cpu);

  debug ("fwrite (\"%s\", %d, %d, 0x%x)\n", buf, len, pieces, dest);
  
  arm32_cpu_return (cpu);
  
  return 0;
}

ARMPROTO (libc_start_main)
{
  debug ("Main: 0x%x\n", R0 (cpu));
  debug ("Argc: %d\n", R1 (cpu));
  debug ("Argv: 0x%x\n", R2 (cpu));

  arm32_cpu_jump (cpu, R0 (cpu));

  R0 (cpu) = R1 (cpu);
  R1 (cpu) = R2 (cpu);
  
  return 0;
}

ARMPROTO (getpagesize)
{
  R0 (cpu) = 4096;
  
  arm32_cpu_return (cpu);
  
  return 0;
}

ARMPROTO (textdomain)
{
  char *textdomain;

  if ((textdomain = arm32_cpu_translate_read (cpu, R0 (cpu))) == NULL)
    EXCEPT (ARM32_EXCEPTION_DATA);

  debug ("Set text domain to \"%s\"\n", textdomain);

  arm32_cpu_return (cpu);

  return 0;
}

ARMPROTO (cxa_atexit)
{
  debug ("Register atexit() handler: 0x%x\n", R0 (cpu));

  arm32_cpu_return (cpu);

  return 0;
}

struct option_arm32
{
  uint32_t pname;
  uint32_t has_arg;
  uint32_t pflag;
  uint32_t val;
}
  __attribute__ ((packed));

ARMPROTO (getopt_long)
{
  uint32_t *stack;
  uint32_t *argv;
  char **argv_copy;
  char *ptr;
  char *optstring;
  struct option_arm32 *longopts;
  
  int32_t *longindex;
  int longopts_count;
  
  struct option *native_longopts;
  
  int i;
  
  /*        int getopt_long(int argc, char * const argv[],
                  const char *optstring,
                  const struct option *longopts, int *longindex);
  */

  debug ("Argc: %d\n", R0 (cpu));

  if ((argv = arm32_cpu_translate_read (cpu, R1 (cpu))) == NULL)
    EXCEPT (ARM32_EXCEPTION_DATA);

  if ((optstring = arm32_cpu_translate_read (cpu, R2 (cpu))) == NULL)
    EXCEPT (ARM32_EXCEPTION_DATA);

  if ((longopts = arm32_cpu_translate_read (cpu, R3 (cpu))) == NULL)
    EXCEPT (ARM32_EXCEPTION_DATA);

  if ((stack = arm32_cpu_translate_read (cpu, SP (cpu))) == NULL)
    EXCEPT (ARM32_EXCEPTION_DATA);

  longindex = arm32_cpu_translate_read (cpu, *stack);
  
  /* Build longopts. This is inherently dangerous, please CHECK if we have memory to perform this */
  longopts_count = 0;

  while (longopts[longopts_count].pname)
    ++longopts_count;

  debug ("%d longopts!\n", longopts_count);

  if ((argv_copy = calloc (R0 (cpu) + 1, sizeof (char *))) == NULL)
  {
    error ("getopt_long: no memory to allocate argv\n");

    EXCEPT (ARM32_EXCEPTION_DATA);
  }

  for (i = 0; i < R0 (cpu); ++i)
    argv_copy[i] = arm32_cpu_translate_read (cpu, argv[i]);
  
  if ((native_longopts = calloc (longopts_count + 1, sizeof (struct option))) == NULL)
  {
    error ("getopt_long: no memory to allocate long options\n");

    free (argv_copy);
    
    EXCEPT (ARM32_EXCEPTION_DATA);
  }

  for (i = 0; i < longopts_count; ++i)
  {
    if ((native_longopts[i].name = arm32_cpu_translate_read (cpu, longopts[i].pname)) == NULL)
    {
      error ("getopt_long: invalid pointer in longopt #%d\n", i);
      error ("  Pname: 0x%x\n", longopts[i].pname);
      error ("  Pflag: 0x%x\n", longopts[i].pflag);
      error ("  Has arg: 0x%x\n", longopts[i].has_arg);
      error ("  Val: '%c'\n", longopts[i].val);
      
      free (argv_copy);
      free (native_longopts);

      EXCEPT (ARM32_EXCEPTION_DATA);
    }

    native_longopts[i].has_arg = longopts[i].has_arg;
    native_longopts[i].val     = longopts[i].val;
    native_longopts[i].flag    = arm32_cpu_translate_read (cpu, longopts[i].pflag);

    /* This one may be NULL */
    debug ("  Register longopt: \"%s\" --> '%c'\n", native_longopts[i].name, native_longopts[i].val);
  }
  
  R0 (cpu) = getopt_long (R0 (cpu), argv_copy, optstring, native_longopts, longindex);

  /* Update optind */
  *arm_optind = optind;
  
  free (argv_copy);
  free (native_longopts);

  arm32_cpu_return (cpu);

  return 0;
}

ARMPROTO (__fxstat64)
{
  struct arm32_stat64 *ptr;
  struct stat sbuf;
  
  debug ("__fxtstat64: version #%d (fd: %d)\n", R0 (cpu), R1 (cpu));

  if ((ptr = arm32_cpu_translate_read (cpu, R2 (cpu))) == NULL)
    EXCEPT (ARM32_EXCEPTION_DATA);

  if ((R0 (cpu) = fstat (R1 (cpu), &sbuf)) != -1)
  {
#define CF(name) ptr->JOIN (ast_, name) = sbuf.JOIN (st_, name)
    CF (dev); CF (mode); CF (nlink); CF (uid); CF (gid);
    CF (rdev); CF (size); CF (blksize); CF (blocks);
    CF (atime); CF (mtime); CF (ctime); CF (ino);

#undef CF
  }

  arm32_cpu_return (cpu);

  return 0;
}

ARMPROTO (memmove)
{
  const void *src;
  void *dst;
  uint32_t size;

  size = R2 (cpu);
  
  if ((dst = arm32_cpu_translate_write_size (cpu, R0 (cpu), size)) == NULL ||
      (src = arm32_cpu_translate_read_size  (cpu, R1 (cpu), size)) == NULL)
    EXCEPT (ARM32_EXCEPTION_DATA);

  memmove (dst, src, size);

  arm32_cpu_return (cpu);

  return 0;
}

ARMPROTO (memcpy)
{
  const void *src;
  void *dst;
  uint32_t size;

  size = R2 (cpu);
  
  if ((dst = arm32_cpu_translate_write_size (cpu, R0 (cpu), size)) == NULL ||
      (src = arm32_cpu_translate_read_size  (cpu, R1 (cpu), size)) == NULL)
    EXCEPT (ARM32_EXCEPTION_DATA);
  
  memcpy (dst, src, size);

  arm32_cpu_return (cpu);

  return 0;
}

ARMPROTO (memset)
{
  void *dst;
  uint32_t size;

  size = R2 (cpu);
  
  if ((dst = arm32_cpu_translate_write_size (cpu, R0 (cpu), size)) == NULL)
    EXCEPT (ARM32_EXCEPTION_DATA);
  
  memset (dst, R1 (cpu), size);

  arm32_cpu_return (cpu);

  return 0;
}


ARMPROTO (strncmp)
{
  const char *s1, *s2;

  if ((s1 = arm32_cpu_translate_read (cpu, R0 (cpu))) == NULL ||
      (s2 = arm32_cpu_translate_read (cpu, R1 (cpu))) == NULL)
    EXCEPT (ARM32_EXCEPTION_DATA);

  debug ("Strncmp: \"%s\", \"%s\", %d\n", s1, s2, R2 (cpu));
  
  R0 (cpu) = strncmp (s1, s2, R2 (cpu));

  arm32_cpu_return (cpu);

  return 0;
}

ARMPROTO (open64)
{
  char *file;
  
  if ((file = arm32_cpu_translate_read (cpu, R0 (cpu))) == NULL)
    EXCEPT (ARM32_EXCEPTION_DATA);

  debug ("Open file: \"%s\", flags %d, mode 0%03o\n",
	  file, R1 (cpu), R2 (cpu));

  R0 (cpu) = open (file, R1 (cpu), R2 (cpu));

  *arm_errno = errno;
  
  arm32_cpu_return (cpu);

  return 0;
}

ARMPROTO (__errno_location)
{
  R0 (cpu) = arm_errno_virt;

  arm32_cpu_return (cpu);

  return 0;
}

const char *
arm32_cpu_fprintf_pieces (struct arm32_cpu *cpu, FILE *output, int index, const char *fmt)
{
  uint32_t *stack;
  uint32_t arg;
  const char *first_fmt, *next_fmt;
  char fmtchar;
  int i, modifiers;
  char *fmtcopy;
  
  if (index < 4)
    arg = REG (cpu, index);
  else
  {
    if ((stack = arm32_cpu_translate_read (cpu, SP (cpu) + (index - 4) * sizeof (uint32_t))) == NULL)
      return NULL;
    
    arg = *stack;
  }

  if ((first_fmt = strchr (fmt, '%')) == NULL)
  {
    fprintf (output, "%s", fmt);
    next_fmt = fmt + strlen (fmt);
  }
  else
  {
    /* TODO: this doesn't support long-long argument. Please fix */
    for (i = first_fmt - fmt + 1; i < strlen (fmt); ++i)
      if (strchr ("%sdifgxXoup", fmtchar = fmt[i]) != NULL)
	break;

    if (fmt[i] == '0')
    {
      fprintf (output, "%s", fmt);
      next_fmt = &fmt[i];
    }
    else
    {
      next_fmt = &fmt[i + 1];

      if ((fmtcopy = malloc (i + 2)) == NULL)
	return NULL;

      memcpy (fmtcopy, fmt, i + 1);
      
      fmtcopy[i + 1] = 0;

      if (fmtchar == 's')
	fprintf (output, fmtcopy, (char *) arm32_cpu_translate_read (cpu, arg));
      else
	fprintf (output, fmtcopy, arg);
      
      free (fmtcopy);
    }
  }

  return next_fmt;
}

ARMPROTO (error)
{
  char *errfmt;
  const char *ptr;
  int n;
  
  if ((errfmt = arm32_cpu_translate_read (cpu, R2 (cpu))) == NULL)
    EXCEPT (ARM32_EXCEPTION_DATA);

  ptr = errfmt;

  n = 3;

  fprintf (stderr, "error: ");
  
  do
  {
    ptr = arm32_cpu_fprintf_pieces (cpu, stderr, n++, ptr);
  }
  while (*ptr);

  fprintf (stderr, ": %s\n", strerror (*arm_errno));

  if (R0 (cpu))
    exit (R0 (cpu));
  
  arm32_cpu_return (cpu);

  return 0;
}

ARMPROTO (posix_fadvise64)
{
  debug ("File access advice:\n");
  debug ("  Fd: %d\n", R0 (cpu));
  debug ("  Offset: 0x%x\n", R1 (cpu));
  debug ("  Len: %d\n", R2 (cpu));
  debug ("  Advice: %d\n", R3 (cpu));

  arm32_cpu_return (cpu);

  return 0;
}

void
__arm32_stdlib_malloc_segment_dtor (void *data, void *phys, uint32_t size)
{
  free (phys);
}

ARMPROTO (free)
{
  uint32_t addr = R0 (cpu);

  int i;
  
  for (i = 0; i < cpu->segment_count; ++i)
    if (cpu->segment_list[i] != NULL)
      if (cpu->segment_list[i]->virt == addr &&
          cpu->segment_list[i]->dtor == __arm32_stdlib_malloc_segment_dtor)
      {
        arm32_segment_destroy (cpu->segment_list[i]);
        cpu->segment_list[i] = NULL;

        arm32_cpu_return (cpu);

        return 0;
      }

  error ("free: unmapped address 0x%x\n", addr);

  EXCEPT (ARM32_EXCEPTION_DATA);
}

ARMPROTO (malloc)
{
  void *mem;
  uint32_t virt;
  struct arm32_segment *seg;
  
  debug ("Memory allocation of %d bytes\n", R0 (cpu));

  if ((virt = arm32_cpu_find_region (cpu, R0 (cpu), 16)) == -1)
  {
    debug ("  Address space is full!\n");

    goto fail;
  }

  if ((mem = calloc (1, R0 (cpu))) == NULL)
  {
    debug ("  Too much!\n");

    goto fail;
  }

  if ((seg = arm32_segment_new (virt, mem, R0 (cpu), SA_R | SA_W)) == NULL)
  {
    debug ("  Cannot allocate segment!\n");
    
    free (mem);

    goto fail;
  }

  arm32_segment_set_dtor (seg, __arm32_stdlib_malloc_segment_dtor, NULL);
  
  if (arm32_cpu_add_segment (cpu, seg) == -1)
  {
    debug ("  Cannot add segment to CPU!\n");
    
    arm32_segment_destroy (seg);

    goto fail;
  }

  debug ("malloc: allocated block of %d bytes in virtual address 0x%x\n", R0 (cpu), virt);
  
  R0 (cpu) = virt;

  arm32_cpu_return (cpu);
  
  return 0;
  
fail:
  
  *arm_errno = ENOMEM;
  
  R0 (cpu) = 0;
  
  arm32_cpu_return (cpu);
  
  return 0;
}

ARMPROTO (dcgettext)
{
  debug ("dcgettext 0x%x\n", R1 (cpu));
  
  arm32_cpu_return (cpu);

  R0 (cpu) = R1 (cpu);
  
  return 0;
}

ARMPROTO (__fprintf_chk)
{
  char *fmt;
  int n;
  const char *ptr;
  
  if ((fmt = arm32_cpu_translate_read (cpu, R2 (cpu))) == NULL)
  {
    error ("Cannot translate 0x%x\n", R2 (cpu));
    EXCEPT (ARM32_EXCEPTION_DATA);
  }
  
  ptr = fmt;

  n = 3;

  do
  {
    ptr = arm32_cpu_fprintf_pieces (cpu, stdout, n++, ptr);
  }
  while (*ptr);

  arm32_cpu_return (cpu);
  
  return 0;  
}

ARMPROTO (__printf_chk)
{
  char *fmt;
  int n;
  const char *ptr;
  
  if ((fmt = arm32_cpu_translate_read (cpu, R1 (cpu))) == NULL)
  {
    error ("Cannot translate 0x%x\n", R1 (cpu));
    EXCEPT (ARM32_EXCEPTION_DATA);
  }
  
  ptr = fmt;

  n = 2;

  do
  {
    ptr = arm32_cpu_fprintf_pieces (cpu, stdout, n++, ptr);
  }
  while (*ptr);

  arm32_cpu_return (cpu);
  
  return 0;  
}

ARMPROTO (fputs_unlocked)
{
  char *s;

  if ((s = arm32_cpu_translate_read (cpu, R0 (cpu))) == NULL)
    EXCEPT (ARM32_EXCEPTION_DATA);
  
  fputs (s, stdout);

  arm32_cpu_return (cpu);

  return 0;
}

ARMPROTO (read)
{
  /* Read is tricky as we need to check if we actually have space for this */

  int fd;
  void *buf;
  uint32_t size;

  fd = R0 (cpu);
  size = R2 (cpu);
  
  if ((buf = arm32_cpu_translate_write_size (cpu, R1 (cpu), size)) == NULL)
  {
    R0 (cpu) = -1;

    *arm_errno = EFAULT;
  }
  else
    if ((R0 (cpu) = read (fd, buf, size)) == -1)
      *arm_errno = EFAULT;

  
  arm32_cpu_return (cpu);
  
  return 0;
}

ARMPROTO (write)
{
  int fd;
  const void *buf;
  uint32_t size;

  fd = R0 (cpu);
  size = R2 (cpu);

  if ((buf = arm32_cpu_translate_read_size (cpu, R1 (cpu), size)) == NULL)
  {
    R0 (cpu) = -1;

    *arm_errno = EFAULT;
  }
  else
    if ((R0 (cpu) = write (fd, buf, size)) == -1)
      *arm_errno = EFAULT;

  arm32_cpu_return (cpu);
  
  return 0;
}

ARMPROTO (close)
{
  R0 (cpu) = close (R0 (cpu));

  arm32_cpu_return (cpu);

  return 0;
}

ARMPROTO (exit)
{
  exit (R0 (cpu));

  return 0;
}

void
arm32_init_stdlib_hooks (struct arm32_cpu *cpu)
{
  struct arm32_elf *elf = (struct arm32_elf *) cpu->data;
  int optind_idx;

  arm_optind = &optind;
  
  if ((optind_idx = arm32_cpu_get_symbol_index (cpu, "optind")) != -1)
    if ((arm_optind = arm32_cpu_translate_read (cpu, elf->symtab[optind_idx].st_value)) == NULL)
      arm_optind = &optind;

  arm32_cpu_override_symbol (cpu, "memset", ARMSYM (memset), NULL);
  arm32_cpu_override_symbol (cpu, "memcpy", ARMSYM (memcpy), NULL);
  arm32_cpu_override_symbol (cpu, "memmove", ARMSYM (memmove), NULL);
  arm32_cpu_override_symbol (cpu, "exit", ARMSYM (exit), NULL);
  arm32_cpu_override_symbol (cpu, "write", ARMSYM (write), NULL);
  arm32_cpu_override_symbol (cpu, "read", ARMSYM (read), NULL);
  arm32_cpu_override_symbol (cpu, "close", ARMSYM (close), NULL);
  arm32_cpu_override_symbol (cpu, "fputs_unlocked", ARMSYM (fputs_unlocked), NULL);
  arm32_cpu_override_symbol (cpu, "__printf_chk", ARMSYM (__printf_chk), NULL);
  arm32_cpu_override_symbol (cpu, "__fprintf_chk", ARMSYM (__fprintf_chk), NULL);
  arm32_cpu_override_symbol (cpu, "dcgettext", ARMSYM (dcgettext), NULL);
  arm32_cpu_override_symbol (cpu, "malloc", ARMSYM (malloc), NULL);
  arm32_cpu_override_symbol (cpu, "free", ARMSYM (free), NULL);
  
  arm32_cpu_override_symbol (cpu, "posix_fadvise64", ARMSYM (posix_fadvise64), NULL);
  arm32_cpu_override_symbol (cpu, "error", ARMSYM (error), NULL);
  arm32_cpu_override_symbol (cpu, "__errno_location", ARMSYM (__errno_location), NULL);
  arm32_cpu_override_symbol (cpu, "open64", ARMSYM (open64), NULL);
  arm32_cpu_override_symbol (cpu, "strncmp", ARMSYM (strncmp), NULL);
  
  arm32_cpu_override_symbol (cpu, "__fxstat64", ARMSYM (__fxstat64), NULL);
  arm32_cpu_override_symbol (cpu, "getopt_long", ARMSYM (getopt_long), NULL);
  arm32_cpu_override_symbol (cpu, "__cxa_atexit", ARMSYM (cxa_atexit), NULL);
  arm32_cpu_override_symbol (cpu, "textdomain", ARMSYM (textdomain), NULL);
  arm32_cpu_override_symbol (cpu, "__libc_start_main", ARMSYM (libc_start_main), NULL);
  arm32_cpu_override_symbol (cpu, "getpagesize", ARMSYM (getpagesize), NULL);
  arm32_cpu_override_symbol (cpu, "strrchr", ARMSYM (strrchr), NULL);
  arm32_cpu_override_symbol (cpu, "fwrite", ARMSYM (fwrite), NULL);
  arm32_cpu_override_symbol (cpu, "setlocale", ARMSYM (setlocale), NULL);
  arm32_cpu_override_symbol (cpu, "bindtextdomain", ARMSYM (bindtextdomain), NULL);
}
