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

#ifndef _ARM_WATCH_H
#define _ARM_WATCH_H

#include "arm_cpu.h"

#define ARM32_WATCHPOINT_REG    0
#define ARM32_WATCHPOINT_MEMORY 1
#define ARM32_WATCHPOINT_STEP   2
#define ARM32_WATCHPOINT_INST   3
#define ARM32_WATCHPOINT_BRANCH 4

#define ARM32_WATCHPOINT_PRE_EXEC  1
#define ARM32_WATCHPOINT_POST_EXEC 2

struct arm32_watchpoint
{
  int type;    /* Watchpoint type */
  int when;    /* When to trigger it */
  int enabled; /* Is it enabled */
  
  char *name;  /* Name of the watchpoint */

  uint32_t mask; /* Register / instruction mask */

  uint32_t prev; /* For address watchpoint: previous value */
  union
  {
    uint32_t addr; /* Address to watch */
    uint32_t inst; /* Instruction */
  };
  
  uint32_t *cached_phys; /* Cached translation for memory watchpoint */
  uint16_t affected; /* Affected registers */
  
  int delay; /* Watchpoint delay before activation */
  int reset; /* Times before resetting */

  void *data; /* Callback data */
  
  int (*callback) (struct arm32_cpu *, struct arm32_watchpoint *, void *);

  int backidx; /* Back index in watchpoint list */
};

struct arm32_watchpoint_set
{
  uint16_t regmask; /* Accumulative register mask */
  
  struct arm32_regs regs_saved;

  PTR_LIST (struct arm32_watchpoint, watchpoint);
};

#endif /* _ARM_WATCH_H */
