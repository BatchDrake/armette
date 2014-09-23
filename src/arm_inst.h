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


#ifndef _ARM_INST_H
#define _ARM_INST_H

#include <util.h>

#include "arm_cpu.h"

#define COND_EQ 0
#define COND_NE 1
#define COND_HS 2
#define COND_LO 3
#define COND_MI 4
#define COND_PL 5
#define COND_VS 6
#define COND_VC 7
#define COND_HI 8
#define COND_LS 9
#define COND_GE 10
#define COND_LT 11
#define COND_GT 12
#define COND_LE 13
#define COND_AL 14
#define COND_NV 15

#define ARM32_DATA_AND 0
#define ARM32_DATA_EOR 1
#define ARM32_DATA_SUB 2
#define ARM32_DATA_RSB 3
#define ARM32_DATA_ADD 4
#define ARM32_DATA_ADC 5
#define ARM32_DATA_SBC 6
#define ARM32_DATA_RSC 7
#define ARM32_DATA_TST 8
#define ARM32_DATA_TEQ 9
#define ARM32_DATA_CMP 10
#define ARM32_DATA_CMN 11
#define ARM32_DATA_ORR 12
#define ARM32_DATA_MOV 13
#define ARM32_DATA_BIC 14
#define ARM32_DATA_MVN 15

#define EXRVAL(code) -((code) + 1)
#define EXCEPT(code) return EXRVAL (code)
#define EXCODE(ret) (-(ret) - 1)

#define IF(inst) JOIN (arm32_inst_, inst)
#define IFPROTO(inst) int IF(inst) (struct arm32_cpu *cpu, uint32_t instruction)

struct arm32_inst
{
  uint32_t mask;
  uint32_t opcode;

  int (*callback) (struct arm32_cpu *, uint32_t);
};

const struct arm32_inst *arm32_inst_decode (struct arm32_cpu *, uint32_t);

#endif /* _ARM_INST_H */
