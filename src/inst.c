#include "arm_cpu.h"
#include "arm_inst.h"

IFPROTO (data);
IFPROTO (multiply);
IFPROTO (longmul);
IFPROTO (swap);
IFPROTO (lssingle);
IFPROTO (lsmultiple);
IFPROTO (halftrans);
IFPROTO (branch);
IFPROTO (branchex);
IFPROTO (codtrans);
IFPROTO (codoper);
IFPROTO (cortrans);
IFPROTO (swi);
IFPROTO (suxt);
IFPROTO (subfx);

/* SBFX:
   cond 0111101 widthm1 Rd 1sb 101 Rn
   UBFX:
   cond 0111111 widthm1 Rd 1sb 101 Rn */
  

static const struct arm32_inst inst_list[15] =
{
  {0b111111111111111111111111, 0b000100101111111111110001, IF (branchex)},
  {0b111000000000000011111001, 0b000000000000000000001001, IF (halftrans)},
  {0b111110100000000000000111, 0b011110100000000000000101, IF (subfx)},
  {0b111110000000000000001111, 0b011010000000000000000111, IF (suxt)},
  {0b111111000000000000001111, 0b000000000000000000001001, IF (multiply)},
  {0b111110000000000000001111, 0b000010000000000000001001, IF (longmul)},
  {0b110000000000000000000000, 0b000000000000000000000000, IF (data)},
  {0b111110110000000011111111, 0b000100000000000000001001, IF (swap)},
  {0b110000000000000000000000, 0b010000000000000000000000, IF (lssingle)},
  {0b111000000000000000000000, 0b100000000000000000000000, IF (lsmultiple)},
  {0b111000000000000000000000, 0b101000000000000000000000, IF (branch)},
  {0b111000000000000000000000, 0b110000000000000000000000, IF (codtrans)},
  {0b111100000000000000010000, 0b111000000000000000000000, IF (codoper)},
  {0b111100000000000000010000, 0b111000000000000000010000, IF (cortrans)},
  {0b111100000000000000000000, 0b111100000000000000000000, IF (swi)}
};


static inline uint32_t
arm32_compute_operand2 (struct arm32_cpu *cpu, int is_imm, uint16_t operand2)
{
  uint32_t value;
  uint32_t amount;
  uint32_t type;
  uint8_t old_c;
  
  type = UINT32_GET_FIELD (operand2, 5, 2);
  
  if (is_imm)
  {
    value = ror32 (UINT32_GET_FIELD (operand2, 0, 8), 2 * UINT32_GET_FIELD (operand2, 8, 4));
  }
  else
  {
    value = REG (cpu, UINT32_GET_FIELD (operand2, 0, 4));
    
    if (operand2 & (1 << 4)) /* Shifted register */
      amount = (uint8_t) REG (cpu, UINT32_GET_FIELD (operand2, 8, 4));
    else
    {
      if ((amount = UINT32_GET_FIELD (operand2, 7, 5)) == 0 && type != BS_SLL)
        amount = 32;
    }
      
    switch (type)
    {
    case BS_SLL:
      if (amount > 0)
        cpu->c = (value >> (32 - amount)) & 1;
      else if (amount > 32)
        cpu->c = 0;
      
      value <<= amount;
      break;

    case BS_SLR:
      cpu->c = (value >> (amount - 1)) & 1;
      value >>= amount;

      break;

    case BS_SAR:
      if (amount >= 32)
      {
        cpu->c = value >> 31;
        value = cpu->c ? 0xffffffff : 0;
      }
      else
      {
        cpu->c = (value >> (amount - 1)) & 1;
        value = (int) value >> amount;
      }

      break;

    case BS_ROR:
      if (amount > 32)
        amount = ((amount - 1) & 31) + 1;
      
      if (amount == 32) /* RRX */
      {
        old_c = IF_C (cpu);
        
        cpu->c = value & 1;
        
        value = (value >> 1) | (old_c << 31);
      }
      else
      {
        cpu->c = (value >> (amount - 1)) & 1;
        value = ror32 (value, amount);
      }
      
      break;
    }
  }

  return value;
}

/* 10: UXTAB
   00: UXTAB16
   11: UXTAH
   10 (rn 15): UXTB
   00 (rn 15): UXTB16
   11 (rn 15): UXTH
*/

IFPROTO (subfx)
{
  uint32_t widthm1 = UINT32_GET_FIELD (instruction, 16, 5);
  uint32_t rd      = UINT32_GET_FIELD (instruction, 12, 4);
  uint32_t lsb     = UINT32_GET_FIELD (instruction,  7, 5);
  uint32_t rn      = UINT32_GET_FIELD (instruction,  0, 4);
  uint32_t nosign  = UINT32_GET_FIELD (instruction, 22, 1);
  uint32_t result;
  
  
  result = UINT32_GET_FIELD (REG (cpu, rn), lsb, widthm1 + 1);

  if (!nosign && result & (1 << widthm1))
    result |= ~((1 << widthm1) - 1);

  REG (cpu, rd) = result;

  debug ("Extract (lsb = %d, width = %d bits, sign: %s) from 0x%x: 0x%x\n", lsb, widthm1 + 1, nosign ? "no" : "yes", REG (cpu, rn), result);
  
  return 0;
}

IFPROTO (suxt)
{
  uint32_t xtype  = UINT32_GET_FIELD (instruction, 20, 2);
  uint32_t rn     = UINT32_GET_FIELD (instruction, 16, 4);
  uint32_t rd     = UINT32_GET_FIELD (instruction, 12, 4);
  uint32_t rotate = UINT32_GET_FIELD (instruction, 10, 2);
  uint32_t sbz    = UINT32_GET_FIELD (instruction,  8, 2);
  uint32_t rm     = UINT32_GET_FIELD (instruction,  0, 4);
  uint32_t op2    = ror32 (REG (cpu, rm), rotate << 3);
  uint32_t op1    = rn == 15 ? 0 : REG (cpu, rn);
  uint32_t result;
  uint32_t nosign = UINT32_GET_FIELD (instruction, 22, 1);
  
  switch (xtype)
  {
    /* UXTAB16 */
  case 0:
    op2 &= 0xff00ff;

    if (nosign)
    {
      UINT32_SET_FIELD (result,  0, 16, (uint16_t) (UINT32_GET_FIELD (op1,  0, 16) + UINT32_GET_FIELD (op2,  0, 16)));
      UINT32_SET_FIELD (result, 16, 16, (uint16_t) (UINT32_GET_FIELD (op1, 16, 16) + UINT32_GET_FIELD (op2, 16, 16)));
    }
    else
    {
      UINT32_SET_FIELD (result,  0, 16, (uint16_t) (UINT32_GET_FIELD (op1,  0, 16) + (int8_t) UINT32_GET_FIELD (op2,  0, 8)));
      UINT32_SET_FIELD (result, 16, 16, (uint16_t) (UINT32_GET_FIELD (op1, 16, 16) + (int8_t) UINT32_GET_FIELD (op2, 16, 8)));
    }
    
    break;

    /* UXTAB */
  case 2:
    op2 = nosign ? op2 & (uint8_t) op2 : (int8_t) op2;
    result = op1 + op2;
    break;

    /* UXTAH */
  case 3:
    op2 = nosign ? (uint16_t) op2 : (int16_t) op2;
    result = op1 + op2;
    break;
    
  }

  REG (cpu, rd) = result;
  
  debug ("%s extend (r%d = extend (r%d, %d) + r%d)\n", nosign ? "Zero" : "Sign", rd, rm, xtype, rn);
  debug ("  op1: 0x%08x\n", op1);
  debug ("  op2: 0x%08x\n", op2);
  debug ("  result: 0x%08x\n", result);

  return 0;
}

IFPROTO (data)
{
  uint32_t is_imm = UINT32_GET_FIELD (instruction, 25, 1);
  uint32_t opcode = UINT32_GET_FIELD (instruction, 21, 4);
  uint32_t ccodes = UINT32_GET_FIELD (instruction, 20, 1);
  uint32_t rn     = UINT32_GET_FIELD (instruction, 16, 4);
  uint32_t rd     = UINT32_GET_FIELD (instruction, 12, 4);
  uint32_t oper2  = UINT32_GET_FIELD (instruction, 0, 12);
  uint32_t nowrite = 0;
  static char *ops[] = {"and", "eor", "sub", "rsb", "add", "adc", "sbc", "rsc", "tst", "teq", "cmp", "cmn", "orr", "mov", "bic", "mvn"};
  uint32_t op1, op2;

  uint32_t tmp;
  uint32_t result;

  op1 = REG (cpu, rn);
  op2 = arm32_compute_operand2 (cpu, is_imm, oper2);

  debug ("data instruction (r%d = %s%s r%d, {0x%x}) [instruction: 0x%08x, opcode: %d]\n", rd, opcode == ARM32_DATA_TST && !ccodes ? "movw" : ops[opcode], ccodes ? "s" : "", rn, oper2, instruction, opcode);
  
  switch (opcode)
  {
  case ARM32_DATA_MVN:
    op2 = ~op2;
    
  case ARM32_DATA_MOV:
    result = op2;

    break;

    
  case ARM32_DATA_BIC:
    result = op1 & ~op2;
    break;
    
  case ARM32_DATA_TST:
    if (!ccodes)
    {
      /* Actually, this is a movw instruction, introduced in ARM6 */

      result = oper2 + (rn << 12);
      
      break;
    }
    else
      nowrite = 1; /* Regular TST instruction */

  case ARM32_DATA_AND:
    result = op1 & op2;
    break;

  case ARM32_DATA_ORR:
    result = op1 | op2;
    break;

  case ARM32_DATA_TEQ:
    nowrite = 1;
    
  case ARM32_DATA_EOR:
    result = op1 ^ op2;
    break;

  case ARM32_DATA_RSB:
    tmp = op1;
    op1 = op2;
    op2 = tmp;

  case ARM32_DATA_CMP:
  case ARM32_DATA_SUB:
    debug ("  Substract 0x%x - 0x%x\n", op1, op2);
    
    op2 = -op2;

  case ARM32_DATA_CMN:
  case ARM32_DATA_ADD:
    nowrite = opcode == ARM32_DATA_CMN || opcode == ARM32_DATA_CMP;
    
    result = op1 + op2;

    /* Play with bits instead of this */
    if (opcode == ARM32_DATA_CMN || opcode == ARM32_DATA_ADD)
      cpu->c = op1 >> 31 != result >> 31 || op2 >> 31 != result >> 31;
    else
      cpu->c = result <= op1;
    /* Borrow is the inverted carry */
    
    cpu->v = op1 >> 31 == op2    >> 31 && op1 >> 31 != result >> 31;

    break;
    
  default:
    debug ("Unsupported operation 0x%x\n", opcode);
    EXCEPT (ARM32_EXCEPTION_UNDEF);
  }

  cpu->n = result >> 31;
  cpu->z = result == 0;

  if (!nowrite)
  {
    REG (cpu, rd) = result;

    debug ("  Update r%-2d: 0x%x\n", rd, result);
  }
  
  if (ccodes)
  {
    UINT32_SET_FIELD (CPSR (cpu), CPSR_N_BIT, 1, cpu->n);
    UINT32_SET_FIELD (CPSR (cpu), CPSR_Z_BIT, 1, cpu->z);
    UINT32_SET_FIELD (CPSR (cpu), CPSR_C_BIT, 1, cpu->c);
    UINT32_SET_FIELD (CPSR (cpu), CPSR_V_BIT, 1, cpu->v);

    debug ("  Update condition codes: %c%c%c%c\n", cpu->n ? 'N' : '-', cpu->z ? 'Z' : '-', cpu->c ? 'C' : '-', cpu->v ? 'V' : '-');
  }
  
  return 0;
}

IFPROTO (multiply)
{
  uint32_t accum = UINT32_GET_FIELD (instruction, 21, 1);
  uint32_t ccode = UINT32_GET_FIELD (instruction, 20, 1);
  uint32_t rd    = UINT32_GET_FIELD (instruction, 16, 4);
  uint32_t rn    = UINT32_GET_FIELD (instruction, 12, 4);
  uint32_t rs    = UINT32_GET_FIELD (instruction, 8,  4);
  uint32_t rm    = UINT32_GET_FIELD (instruction, 0,  4);

  uint32_t result;

  result = REG (cpu, rm) * REG (cpu, rs);

  if (accum)
    result += REG (cpu, rn);

  REG (cpu, rd) = result;
  
  if (ccode)
  {
    cpu->n = result >> 31;
    cpu->z = result == 0;
    cpu->c = 0; /* Meaningless */
  }
  
  return 0;
}

IFPROTO (longmul)
{
  debug ("Long multiply instruction issued\n");
  
  EXCEPT (ARM32_EXCEPTION_UNDEF);
}

IFPROTO (swap)
{
  debug ("SWAP instruction issued\n");
  
  EXCEPT (ARM32_EXCEPTION_UNDEF);
}

IFPROTO (lssingle)
{
  int is_imm = !UINT32_GET_FIELD (instruction, 25, 1);
  int preidx = UINT32_GET_FIELD (instruction, 24, 1);
  int up_bit = UINT32_GET_FIELD (instruction, 23, 1);
  int isbyte = UINT32_GET_FIELD (instruction, 22, 1);
  int wrback = UINT32_GET_FIELD (instruction, 21, 1);
  int isload = UINT32_GET_FIELD (instruction, 20, 1);
  int rn     = UINT32_GET_FIELD (instruction, 16, 4);
  int rd     = UINT32_GET_FIELD (instruction, 12, 4);
  int off    = UINT32_GET_FIELD (instruction, 0, 12);
  uint32_t *phaddr;
  
  uint32_t addr;
  
  uint32_t base = REG (cpu, rn);
  uint32_t offset = is_imm ? off : arm32_compute_operand2 (cpu, 0, off);

  struct arm32_segment *seg;
  
  addr = base;

  if (preidx)
    addr += up_bit ? offset : -offset;

  if ((seg = arm32_cpu_lookup_segment (cpu, addr)) == NULL)
  {
    error ("%s: unmapped address 0x%x\n", isload ? "ldr" : "str", addr);

    EXCEPT (ARM32_EXCEPTION_DATA);
  }

  if (arm32_segment_check_access (seg, isload ? SA_R : SA_W) == -1)
  {
    error ("%s: forbidden access to 0x%x\n", isload ? "ldr" : "str", addr);

    EXCEPT (ARM32_EXCEPTION_DATA);
  }

  phaddr = arm32_segment_translate (seg, addr);

  if (isload)
  {
    if (isbyte)
      REG (cpu, rd) = (uint8_t) *phaddr;
    else
      REG (cpu, rd) = *phaddr;

    if (isbyte)
      debug ("load: r%-2d = 0x%02x <-- 0x%08x (r%d)\n", rd, (uint8_t) REG (cpu, rd),  addr, rn);
    else
      debug ("load: r%-2d = 0x%08x <-- 0x%08x (r%d)\n", rd, REG (cpu, rd), addr, rn);
  }
  else
  {
    if (isbyte)
      debug ("stor: r%-2d = 0x%02x --> 0x%08x (r%d)\n", rd, (uint8_t) REG (cpu, rd), addr, rn);
    else
      debug ("stor: r%-2d = 0x%08x --> 0x%08x (r%d)\n", rd, REG (cpu, rd), addr, rn);

    if (isbyte)
      UINT32_SET_FIELD (*phaddr, 0, 8, (uint8_t) REG (cpu, rd));
    else
      *phaddr = REG (cpu, rd);
  }
  
  if (!preidx)
  {
    addr += up_bit ? offset : -offset;

    /* This is a privileged instruction! */
    if (wrback)
      EXCEPT (ARM32_EXCEPTION_DATA);

    wrback = 1;
  }

  if (wrback)
    REG (cpu, rn) = addr;

  return 0;
}

IFPROTO (lsmultiple)
{
  uint32_t preidx = UINT32_GET_FIELD (instruction, 24, 1);
  uint32_t up_bit = UINT32_GET_FIELD (instruction, 23, 1);
  uint32_t psrusr = UINT32_GET_FIELD (instruction, 22, 1);
  uint32_t wrback = UINT32_GET_FIELD (instruction, 21, 1);
  uint32_t isload = UINT32_GET_FIELD (instruction, 20, 1);
  uint32_t rn     = UINT32_GET_FIELD (instruction, 16, 4);
  uint32_t regs   = UINT32_GET_FIELD (instruction, 0, 16);
  uint32_t addr;
  uint32_t *phaddr;

  struct arm32_segment *seg;
  
  int i, j;
  int aborted = 0;
  
  if (isload)
    debug ("load multiple (%c): ", up_bit ? 'U' : 'D');
  else
    debug ("stor multiple (%c): ", up_bit ? 'U' : 'D');

  for (i = 0; i < 16; ++i)
    if (regs & (1 << i))
      debug (" r%d", i);

  if (isload)
    debug (" <-- 0x%x\n", REG (cpu, rn));
  else
    debug (" --> 0x%x\n", REG (cpu, rn));
  
  if (psrusr)
  {
    error ("Load/store multiple: privileged instruction!\n");
    EXCEPT (ARM32_EXCEPTION_DATA);
  }

  addr = REG (cpu, rn);
  
  for (j = 0; j < 16; ++j)
  {
    /* Note: this works ONLY if we don't care about how exceptions are handled. Specification requires r15 to be the last register to be written, and this brokens it. */
    i = isload ? 15 - j : j;
    
    if (regs & (1 << i))
    {
      if (preidx)
	addr += up_bit ? 4 : -4;

      if ((seg = arm32_cpu_lookup_segment (cpu, addr)) == NULL)
      {
	error ("%s: unmapped address 0x%x\n", isload ? "ldm" : "stm", addr);
	
	aborted = 1;

	continue;
      }
      
      if (arm32_segment_check_access (seg, isload ? SA_R : SA_W) == -1)
      {
	error ("%s: forbidden access to 0x%x\n", isload ? "ldm" : "stm", addr);
	
	aborted = 1;

	continue;
      }
      
      phaddr = arm32_segment_translate (seg, addr);
      
      if (!isload)
      {
	*phaddr = REG (cpu, i);
	debug ("  --> Stor 0x%08x fr r%-2d (0x%08x)\n", addr, i, *phaddr);
      }
      else if (!aborted)
      {
	REG (cpu, i) = *phaddr;
	debug ("  --> Load 0x%08x to r%-2d (0x%08x)\n", addr, i, *phaddr);
      }
      
      if (!preidx)
	addr += up_bit ? 4 : -4;
    }
  }
  
  if (wrback && (!isload || !aborted))
    REG (cpu, rn) = addr;

  if (aborted)
    EXCEPT (ARM32_EXCEPTION_DATA);

  return 0;
}

IFPROTO (halftrans)
{
  uint32_t preidx = UINT32_GET_FIELD (instruction, 24, 1);
  uint32_t up_bit = UINT32_GET_FIELD (instruction, 23, 1);
  uint32_t is_imm = UINT32_GET_FIELD (instruction, 22, 1);
  uint32_t wrback = UINT32_GET_FIELD (instruction, 21, 1);
  uint32_t isload = UINT32_GET_FIELD (instruction, 20, 1);
  uint32_t rn     = UINT32_GET_FIELD (instruction, 16, 4);
  uint32_t rd     = UINT32_GET_FIELD (instruction, 12, 4);
  uint32_t offhi  = UINT32_GET_FIELD (instruction, 8,  4);
  uint32_t offlo  = UINT32_GET_FIELD (instruction, 0,  4);
  uint32_t halfw  = UINT32_GET_FIELD (instruction, 5,  1);
  uint32_t signex = UINT32_GET_FIELD (instruction, 6,  1);
  
  uint32_t rm     = offlo;
  
  uint16_t *phaddr;
  
  uint32_t addr;
  
  uint32_t base = REG (cpu, rn);
  uint32_t offset = is_imm ? (offhi << 4) | offlo : REG (cpu, offlo);

  struct arm32_segment *seg;
  
  addr = base;

  if (preidx)
    addr += up_bit ? offset : -offset;

  if ((seg = arm32_cpu_lookup_segment (cpu, addr)) == NULL)
  {
    error ("%s: unmapped address 0x%x\n", isload ? "ldrh" : "strh", addr);

    EXCEPT (ARM32_EXCEPTION_DATA);
  }

  if (arm32_segment_check_access (seg, isload ? SA_R : SA_W) == -1)
  {
    error ("%s: forbidden access to 0x%x\n", isload ? "ldrh" : "strh", addr);

    EXCEPT (ARM32_EXCEPTION_DATA);
  }

  phaddr = arm32_segment_translate (seg, addr);

  debug ("Half transfer with immediate instruction issued\n");
 
  if (isload)
  {
    if (!halfw)
      REG (cpu, rd) = signex ? __extend ((uint8_t) *phaddr, 8) : (uint8_t) *phaddr;
    else
      REG (cpu, rd) = signex ? __extend (*phaddr, 16) : *phaddr;

    if (!halfw)
      debug ("load: r%-2d = 0x%02x <-- 0x%08x (r%d)\n", rd, REG (cpu, rd), addr, rn);
    else
      debug ("load: r%-2d = 0x%04x <-- 0x%08x (r%d)\n", rd, REG (cpu, rd), addr, rn);
  }
  else
  {
    if (signex)
    {
      error ("Sign extension store is not supported\n");

      EXCEPT (ARM32_EXCEPTION_UNDEF);
    }
    
    if (!halfw)
      debug ("stor: r%-2d = 0x%02x --> 0x%08x (r%d)\n", rd, (uint8_t) REG (cpu, rd), addr, rn);
    else
      debug ("stor: r%-2d = 0x%04x --> 0x%08x (r%d)\n", rd, (uint16_t) REG (cpu, rd), addr, rn);
    
    if (!halfw)
      UINT32_SET_FIELD (*phaddr, 0, 8, (uint8_t) REG (cpu, rd));
    else
      *phaddr = REG (cpu, rd);
  }
  
  if (!preidx)
  {
    addr += up_bit ? offset : -offset;
    
    /* This is a privileged instruction! */
    if (wrback)
      EXCEPT (ARM32_EXCEPTION_DATA);
    
    wrback = 1;
  }
  
  if (wrback)
    REG (cpu, rn) = addr;
  
  return 0;
}


IFPROTO (branch)
{
  int link        = UINT32_GET_FIELD (instruction, 24, 1);
  uint32_t offset = __extend (UINT32_GET_FIELD (instruction, 0, 24) << 2, 26);
  uint32_t addr;

  
  addr = PC (cpu) + offset;

  if (link)
    debug ("call procedure (");
  else
    debug ("branch (");
  
  debug ("0x%x)\n", addr);

  if (link)
    LR (cpu) = PC (cpu) - 4;
  
  PC (cpu) = addr;
  
  return 0;
}

IFPROTO (branchex)
{
  uint32_t rn = UINT32_GET_FIELD (instruction, 0, 4);
  uint32_t addr = REG (cpu, rn);

  debug ("branch with exchange on r%d (0x%x)\n", rn, addr);

  if (addr & 3)
  {
    error ("BX to THUMB mode not supported!\n");
    EXCEPT (ARM32_EXCEPTION_UNDEF);
  }
  
  PC (cpu) = addr;

  return 0;
}

IFPROTO (codtrans)
{
  error ("Coprocessor data transfer instruction issued\n");
  
  EXCEPT (ARM32_EXCEPTION_UNDEF);
}

IFPROTO (codoper)
{
  error ("Coprocessor data operation instruction issued\n");
  
  EXCEPT (ARM32_EXCEPTION_UNDEF);
}

IFPROTO (cortrans)
{
  error ("Coprocessor register transfer instruction issued\n");
  
  EXCEPT (ARM32_EXCEPTION_UNDEF);
}

IFPROTO (swi)
{  
  EXCEPT (ARM32_EXCEPTION_SWI);
}

const struct arm32_inst *
arm32_inst_decode (struct arm32_cpu *cpu, uint32_t inst)
{
  int i;

  /* Don't care about condition */
  inst = (inst >> 4) & 0xffffff;
  
  for (i = 0; i < 15; ++i)
  {
    if ((inst & inst_list[i].mask) == inst_list[i].opcode)
      return &inst_list[i];
    
  }
  
  return NULL;
}

