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


#ifndef _ARM_CPU_H
#define _ARM_CPU_H

#include <stdint.h>
#include <util.h>

#define ARM32_IMPORT_HOOK_BASE     0xc00000

#define ARM32_EXCEPTION_RESET 0 /* Never issued */
#define ARM32_EXCEPTION_UNDEF 1 /* Undefined instruction */
#define ARM32_EXCEPTION_SWI   2 /* Software interrupt */
#define ARM32_EXCEPTION_PREF  3 /* Prefetch abort */
#define ARM32_EXCEPTION_DATA  4 /* Data abort */
#define ARM32_EXCEPTION_RESV  5 /* Reserved */
#define ARM32_EXCEPTION_IRQ   6 /* IRQ */
#define ARM32_EXCEPTION_FIQ   7 /* Fast IRQ */
#define ARM32_EXCEPTION_EXIT  8 /* Exit emulator */
#define ARM32_EXCEPTION_TRAP  9 /* Trap */

#define ARM32_DEFAULT_STACK_SIZE   (64 * 1024)
#define ARM32_DEFAULT_STACK_BOTTOM 0xc0000000
#define ARM32_DEFAULT_VDSO_BOTTOM  0xe0000000

#define ARM32_ARMETTE_RETURN_INSTRUCTION 0xefffffff

#define CPSR_N_BIT 31
#define CPSR_Z_BIT 30
#define CPSR_C_BIT 29
#define CPSR_V_BIT 28

#define CPSR_N (1 << CPSR_N_BIT)
#define CPSR_Z (1 << CPSR_Z_BIT)
#define CPSR_C (1 << CPSR_C_BIT)
#define CPSR_V (1 << CPSR_V_BIT)

#define CPSR_I 0x80
#define CPSR_F 0x40
#define CPSR_T 0x20

#define ARMETTE_ERROR   1
#define ARMETTE_WARNING 2
#define ARMETTE_NOTICE  3
#define ARMETTE_DEBUG   4

#ifdef error
#undef error
#endif

#ifdef warning
#undef warning
#endif

#ifdef notice
#undef notice
#endif

#ifdef debug
#undef debug
#endif

#define error(fmt, arg...)   arm32_dbg (ARMETTE_ERROR, fmt, ##arg)
#define warning(fmt, arg...) arm32_dbg (ARMETTE_WARNING, fmt, ##arg)
#define notice(fmt, arg...)  arm32_dbg (ARMETTE_NOTICE, fmt, ##arg)
#define debug(fmt, arg...)   arm32_dbg (ARMETTE_DEBUG, fmt, ##arg)

#define REG(cpu, reg) cpu->regs.r[reg]
#define O_REG(cpu, reg) cpu->wps->regs_saved.r[reg]

#define R0(cpu)  REG (cpu, 0)
#define R1(cpu)  REG (cpu, 1)
#define R2(cpu)  REG (cpu, 2)
#define R3(cpu)  REG (cpu, 3)
#define R4(cpu)  REG (cpu, 4)
#define R5(cpu)  REG (cpu, 5)
#define R6(cpu)  REG (cpu, 6)
#define R7(cpu)  REG (cpu, 7)
#define R8(cpu)  REG (cpu, 8)
#define R9(cpu)  REG (cpu, 9)
#define R10(cpu) REG (cpu, 10)
#define R11(cpu) REG (cpu, 11)
#define R12(cpu) REG (cpu, 12)
#define R13(cpu) REG (cpu, 13)
#define R14(cpu) REG (cpu, 14)
#define R15(cpu) REG (cpu, 15)

#define O_R0(cpu)  O_REG (cpu, 0)
#define O_R1(cpu)  O_REG (cpu, 1)
#define O_R2(cpu)  O_REG (cpu, 2)
#define O_R3(cpu)  O_REG (cpu, 3)
#define O_R4(cpu)  O_REG (cpu, 4)
#define O_R5(cpu)  O_REG (cpu, 5)
#define O_R6(cpu)  O_REG (cpu, 6)
#define O_R7(cpu)  O_REG (cpu, 7)
#define O_R8(cpu)  O_REG (cpu, 8)
#define O_R9(cpu)  O_REG (cpu, 9)
#define O_R10(cpu) O_REG (cpu, 10)
#define O_R11(cpu) O_REG (cpu, 11)
#define O_R12(cpu) O_REG (cpu, 12)
#define O_R13(cpu) O_REG (cpu, 13)
#define O_R14(cpu) O_REG (cpu, 14)
#define O_R15(cpu) O_REG (cpu, 15)


#define BS_SLL 0
#define BS_SLR 1
#define BS_SAR 2
#define BS_ROR 3

#define SP(cpu)  R13 (cpu)
#define LR(cpu)  R14 (cpu)
#define PC(cpu)  R15 (cpu)

#define O_SP(cpu)  O_R13 (cpu)
#define O_LR(cpu)  O_R14 (cpu)
#define O_PC(cpu)  O_R15 (cpu)

#define CPSR(cpu) cpu->regs.cpsr

#define UINT32_MASK(len) (((uint32_t) 1 << ((uint32_t) len)) - 1)
#define UINT32_SHIFT_MASK(off, len) (UINT32_MASK (len) << (uint32_t) (off))

#define UINT32_GET_FIELD(field, off, len) ((((uint32_t) field) >> ((uint32_t) off)) & UINT32_MASK (len))
#define UINT32_SET_FIELD(field, off, len, value) field = (((uint32_t) field) & ~UINT32_SHIFT_MASK (off, len)) | (uint32_t) (((uint32_t) value) << ((uint32_t) off))

static inline uint32_t
__extend (uint32_t orig, int bits)
{
  orig &= UINT32_MASK (bits);

  if (orig & (1 << (bits - 1)))
    orig |= ~UINT32_MASK (bits);

  return orig;
}

static inline uint32_t
rol32 (uint32_t value, int places) 
{ 
  return (value << places) | (value >> (32 - places)); 
} 

static inline uint32_t
ror32 (uint32_t value, int places) 
{ 
  return (value >> places) | (value << 32 - places); 
} 

  
#define IF_N(cpu) (!!(CPSR (cpu) & CPSR_N))
#define IF_Z(cpu) (!!(CPSR (cpu) & CPSR_Z))
#define IF_C(cpu) (!!(CPSR (cpu) & CPSR_C))
#define IF_V(cpu) (!!(CPSR (cpu) & CPSR_V))

#define SA_X 1
#define SA_W 2
#define SA_R 4

struct arm32_segment
{
  uint32_t virt;
  uint32_t size;

  void *phys;

  uint8_t flags;

  void *data;
  void (*dtor) (void *, void *, uint32_t);
};

struct arm32_regs
{
  uint32_t r[16];
  uint32_t cpsr;
};

struct arm32_watchpoint_set;

struct arm32_cpu
{
  struct arm32_regs regs;

  /* This, to be effective, should be a radix tree */
  PTR_LIST (struct arm32_segment, segment);

  /* Temporary fields to store flags */
  unsigned char c:1, z:1, n:1, v:1;
  
  uint32_t next_pc;
  
  void (*vector_table[8]) (struct arm32_cpu *, uint32_t, uint32_t);

  void *data;
  void (*dtor) (void *);

  struct arm32_watchpoint_set *wps;
};

static inline struct arm32_segment *
arm32_cpu_lookup_segment (struct arm32_cpu *cpu, uint32_t virt)
{
  struct arm32_segment *seg;
  int i;

  for (i = 0; i < cpu->segment_count; ++i)
    if ((seg = cpu->segment_list[i]) != NULL)
      if (seg->virt <= virt &&
	  virt < (seg->virt + seg->size))
	return seg;

  return NULL;
}

static inline int
arm32_segment_check_access (struct arm32_segment *seg, uint8_t access)
{
  return ((seg->flags | access) != seg->flags) ? -1 : 0;
}

static inline void *
arm32_segment_translate (struct arm32_segment *seg, uint32_t virt)
{
  return seg->phys + (virt - seg->virt);
}

static inline void *
arm32_cpu_translate_read (struct arm32_cpu *cpu, uint32_t virt)
{
  struct arm32_segment *seg;

  if ((seg = arm32_cpu_lookup_segment (cpu, virt)) == NULL)
    return NULL;

  if (arm32_segment_check_access (seg, SA_R) == -1)
    return NULL;
  
  return arm32_segment_translate (seg, virt);
}

static inline void *
arm32_cpu_translate_write_size (struct arm32_cpu *cpu, uint32_t virt, uint32_t size)
{
  struct arm32_segment *seg;

  if ((seg = arm32_cpu_lookup_segment (cpu, virt)) == NULL)
    return NULL;

  /* Overflow */
  if (seg->virt + seg->size < virt + size)
    return NULL;
  
  if (arm32_segment_check_access (seg, SA_W) == -1)
    return NULL;
  
  return arm32_segment_translate (seg, virt);
}

static inline void *
arm32_cpu_translate_read_size (struct arm32_cpu *cpu, uint32_t virt, uint32_t size)
{
  struct arm32_segment *seg;

  if ((seg = arm32_cpu_lookup_segment (cpu, virt)) == NULL)
    return NULL;

  /* Overflow */
  if (seg->virt + seg->size < virt + size)
    return NULL;
  
  if (arm32_segment_check_access (seg, SA_R) == -1)
    return NULL;
  
  return arm32_segment_translate (seg, virt);
}

struct arm32_cpu *arm32_cpu_new (void);
struct arm32_segment *arm32_segment_new (uint32_t, void *, uint32_t, uint8_t);
void arm32_segment_set_dtor (struct arm32_segment *, void (*) (void *, void *, uint32_t), void *);
int arm32_cpu_add_segment (struct arm32_cpu *, struct arm32_segment *);
int arm32_cpu_remove_segment (struct arm32_cpu *, struct arm32_segment *);
void arm32_segment_destroy (struct arm32_segment *);
void arm32_cpu_destroy (struct arm32_cpu *);
int arm32_cpu_run (struct arm32_cpu *);
int arm32_cpu_callproc (struct arm32_cpu *, uint32_t);
void arm32_cpu_jump (struct arm32_cpu *, uint32_t);
void arm32_cpu_return (struct arm32_cpu *);
int arm32_elf_call_external (struct arm32_cpu *, uint32_t);
uint32_t arm32_cpu_find_region (const struct arm32_cpu *, uint32_t, uint32_t);
void arm32_dbg (unsigned int, const char *, ...);

#endif /* _ARM_CPU_H */
