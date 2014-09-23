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


#ifndef _ARM_ELF_H
#define _ARM_ELF_H

#include <stdint.h>
#include <util.h>
#include <elf.h>

struct arm32_cpu;

struct arm32_elf_symbol_override
{
  char *name;
  int (*callback) (struct arm32_cpu *, const char *name, void *data);
  void *data;
};

struct arm32_elf
{
  void *base;
  size_t size;

  void *stack_base;
  size_t stack_size;
  
  Elf32_Ehdr *ehdr;
  Elf32_Phdr *phdr;

  uint32_t *got; /* Global offset table */

  int symtab_size;
  Elf32_Sym *symtab;
  char *strtab;
  int strtab_size;  
  int symtab_first;
  int symtab_sane;

  PTR_LIST (struct arm32_elf_symbol_override, symbol);
};

struct arm32_cpu *arm32_cpu_new_from_elf (const char *);
int arm32_cpu_get_symbol_index (struct arm32_cpu *, const char *);
int arm32_cpu_define_symbol (struct arm32_elf *, const char *, int, int (*) (struct arm32_cpu *, const char *name, void *data), void *);
int arm32_cpu_override_symbol (struct arm32_cpu *, const char *, int (*) (struct arm32_cpu *, const char *name, void *data), void *);
int arm32_cpu_prepare_main (struct arm32_cpu *, int, char **);
void arm32_init_stdlib_hooks (struct arm32_cpu *);

#endif /* _ARM_ELF_H */
