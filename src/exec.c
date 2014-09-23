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


#include "arm_cpu.h"
#include "arm_inst.h"

extern struct arm32_cpu *curr_cpu;

int
arm32_check_condition (struct arm32_cpu *cpu, uint32_t op)
{
  uint32_t condition = op >> 28;

  switch (condition)
  {
  case COND_EQ:
    return IF_Z (cpu);

  case COND_NE:
    return !IF_Z (cpu);

  case COND_HS:
    return IF_C (cpu);

  case COND_LO:
    return !IF_C (cpu);

  case COND_MI:
    return IF_N (cpu);

  case COND_PL:
    return !IF_N (cpu);

  case COND_VS:
    return IF_V (cpu);

  case COND_VC:
    return !IF_V (cpu);

  case COND_HI:
    return IF_C (cpu) && !IF_Z (cpu);

  case COND_LS:
    return !IF_C (cpu) || IF_Z (cpu);

  case COND_GE:
    return IF_N (cpu) == IF_V (cpu);

  case COND_LT:
    return IF_N (cpu) != IF_V (cpu);

  case COND_GT:
    return (IF_N (cpu) == IF_V (cpu)) && !IF_Z (cpu);

  case COND_LE:
    return (IF_N (cpu) != IF_V (cpu)) || IF_Z (cpu);

  case COND_AL:
    return 1;

  case COND_NV:
    return 0;
  }

  return 0;
}

void
arm32_cpu_jump (struct arm32_cpu *cpu, uint32_t addr)
{
  cpu->next_pc = addr;
}

void
arm32_cpu_return (struct arm32_cpu *cpu)
{
  arm32_cpu_jump (cpu, LR (cpu));
}

int
arm32_inst_fetch (struct arm32_cpu *cpu, uint32_t *dest)
{
  struct arm32_segment *seg;
  uint32_t *inst;

  PC (cpu) = cpu->next_pc;
  
  cpu->next_pc += 4;
  
  if ((seg = arm32_cpu_lookup_segment (cpu, PC (cpu))) == NULL)
  {
    fprintf (stderr, "Code: unmapped $pc: 0x%x\n", PC (cpu));

    EXCEPT (ARM32_EXCEPTION_DATA);
  }

  if (arm32_segment_check_access (seg, SA_R | SA_X) == -1)
  {
    fprintf (stderr, "Code: no text segment in $pc: 0x%x\n", PC (cpu));

    EXCEPT (ARM32_EXCEPTION_DATA);
  }

  inst = arm32_segment_translate (seg, PC (cpu));

  *dest = *inst;

  return 0;
}

int
arm32_inst_execute (struct arm32_cpu *cpu, const struct arm32_inst *inst, uint32_t opcode)
{
  return (inst->callback) (cpu, opcode);
}

int
arm32_cpu_except (struct arm32_cpu *cpu, int except, uint32_t addr, uint32_t code)
{
  if (cpu->vector_table[except] == NULL)
    return -1;

  (cpu->vector_table[except]) (cpu, addr, code);

  return 0;
}

int
arm32_cpu_callproc (struct arm32_cpu *cpu, uint32_t addr)
{
  arm32_cpu_jump (cpu, addr);

  LR (cpu) = ARM32_DEFAULT_VDSO_BOTTOM;

  return arm32_cpu_run (cpu);
}

int
arm32_cpu_run (struct arm32_cpu *cpu)
{
  const struct arm32_inst *inst;
  uint32_t instruction;
  int ret;
  uint32_t sym;

  curr_cpu = cpu;
  
  /* TODO: get a better way to retrieve error codes */
  for (;;)
  {
    if ((ret = arm32_inst_fetch (cpu, &instruction)) < 0)
      if (arm32_cpu_except (cpu, EXCODE (ret), PC (cpu), 0) == -1)
        break;

    if (instruction == ARM32_ARMETTE_RETURN_INSTRUCTION)
    {
      ret = 0;
      break;
    }
    
    if ((inst = arm32_inst_decode (cpu, instruction)) == NULL)
      if (arm32_cpu_except (cpu, ARM32_EXCEPTION_UNDEF, PC (cpu), instruction) == -1)
      {
        curr_cpu = NULL;
	EXCEPT (ARM32_EXCEPTION_UNDEF);
      }
    
    cpu->c = IF_C (cpu);
    cpu->z = IF_Z (cpu);
    cpu->n = IF_N (cpu);
    cpu->v = IF_V (cpu);
    
    PC (cpu) += 8;
    
    if (arm32_check_condition (cpu, instruction))
    {
      ret = arm32_inst_execute (cpu, inst, instruction);
 
      /* Jump happened, readjust PC */
      if (PC (cpu) - 8 != cpu->next_pc - 4)
        cpu->next_pc = PC (cpu);
      else
        PC (cpu) -= 8;

      if (EXCODE (ret) == ARM32_EXCEPTION_SWI)
      {
	sym = instruction & 0xffffff;

	if ((ret = arm32_elf_call_external (cpu, sym)) < 0)
          break;
      }
      else if (ret < 0)
     	if (arm32_cpu_except (cpu, EXCODE (ret), PC (cpu), 0) == -1)
          break;
    }
  }

  curr_cpu = NULL;
  
  return ret;
}


