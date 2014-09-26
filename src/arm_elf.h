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

#define ARMSYM(sname) JOIN (arm32_stdlib_, sname)

#define ARMPROTO(sname) \
  int ARMSYM (sname) (struct arm32_cpu *cpu, const char *name, void *data, uint32_t prev)

#define ARMETTE_OVERRIDE(cpu, name) arm32_cpu_override_symbol (cpu, STRINGIFY (name), ARMSYM (name), NULL);
struct arm32_cpu;

struct arm32_elf_instruction_override
{
  char *name;
  uint32_t prev;
  uint32_t *phys;
  int (*callback) (struct arm32_cpu *, const char *name, void *data, uint32_t prev);
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

  /* Used for dynamic linking */
  Elf32_Sym *symtab;
  int        symtab_size;
  
  char      *strtab;
  int        strtab_size;  

  int        symtab_first;
  int        symtab_sane;

  Elf32_Sym *debug_symtab;
  int        debug_symtab_size;
  
  char      *debug_strtab;
  int        debug_strtab_size;
  
  PTR_LIST (struct arm32_elf_instruction_override, override);
};

struct arm32_cpu *arm32_cpu_new_from_elf (const char *);
int arm32_cpu_get_symbol_index (struct arm32_cpu *, const char *);
int arm32_cpu_define_symbol (struct arm32_elf *, const char *, int, int (*) (struct arm32_cpu *, const char *name, void *data, uint32_t), void *);
int arm32_cpu_override_symbol (struct arm32_cpu *, const char *, int (*) (struct arm32_cpu *, const char *name, void *data, uint32_t), void *);
int arm32_cpu_restore_symbol (struct arm32_cpu *, const char *);
int arm32_cpu_prepare_main (struct arm32_cpu *, int, char **);
void arm32_init_stdlib_hooks (struct arm32_cpu *);
uint32_t arm32_elf_resolve_debug_symbol (struct arm32_elf *, const char *);
int arm32_elf_replace_instruction (struct arm32_elf *elf, const char *name, uint32_t vaddr, int (*callback) (struct arm32_cpu *, const char *name, void *data, uint32_t), void *data);

static inline uint32_t
arm32_cpu_resolve_debug_symbol (struct arm32_cpu *cpu, const char *name)
{
  return arm32_elf_resolve_debug_symbol ((struct arm32_elf *) cpu->data, name);
}

static inline uint32_t
arm32_cpu_override_debug_symbol (struct arm32_cpu *cpu, const char *name, int (*callback) (struct arm32_cpu *, const char *name, void *data, uint32_t), void *data)
{
  uint32_t addr;
  
  if ((addr = arm32_elf_resolve_debug_symbol ((struct arm32_elf *) cpu->data, name)) != 0)
    return arm32_elf_replace_instruction ((struct arm32_elf *) cpu->data, name, addr, callback, data);

  return -1;
}

#endif /* _ARM_ELF_H */
