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

#include <assert.h>

#include <string.h>

#include "arm_cpu.h"
#include "arm_inst.h"
#include "arm_watch.h"

struct arm32_watchpoint *
arm32_watchpoint_new (const char *name, int type, int when)
{
  struct arm32_watchpoint *new;

  if ((new = calloc (1, sizeof (struct arm32_watchpoint))) == NULL)
    return NULL;

  if ((new->name = strdup (name)) == NULL)
  {
    free (new);

    return NULL;
  }

  new->enabled = 1;
  new->type = type;
  new->when = when;

  return new;
}

void
arm32_watchpoint_destroy (struct arm32_watchpoint *wp)
{
  free (wp->name);
  free (wp);
}

struct arm32_watchpoint_set *
arm32_watchpoint_set_new (void)
{
  struct arm32_watchpoint_set *new;

  if ((new = calloc (1, sizeof (struct arm32_watchpoint_set))) == NULL)
    return NULL;

  return new;
}

void
arm32_watchpoint_set_destroy (struct arm32_watchpoint_set *wps)
{
  int i;

  for (i = 0; i < wps->watchpoint_count; ++i)
    if (wps->watchpoint_list[i] != NULL)
      arm32_watchpoint_destroy (wps->watchpoint_list[i]);

  if (wps->watchpoint_list != NULL)
    free (wps->watchpoint_list);

  free (wps);
}

int
arm32_watchpoint_register (struct arm32_watchpoint_set *wps, struct arm32_watchpoint *wp)
{
  int backidx;
  
  if ((backidx = PTR_LIST_APPEND_CHECK (wps->watchpoint, wp)) == -1)
    return -1;

  wp->backidx = backidx;

  if (wp->type == ARM32_WATCHPOINT_REG)
    wps->regmask |= wp->mask;

  return 0;
}

static void
arm32_watchpoint_set_recalc_regmask (struct arm32_watchpoint_set *wps)
{
  int i;

  uint16_t mask = 0;

  for (i = 0; i < wps->watchpoint_count; ++i)
    if (wps->watchpoint_list[i] != NULL)
      if (wps->watchpoint_list[i]->type == ARM32_WATCHPOINT_REG)
	mask |= wps->watchpoint_list[i]->mask;

  wps->regmask = mask;
}

void
arm32_watchpoint_disable (struct arm32_watchpoint *wp)
{
  wp->enabled = 0;
}

void
arm32_watchpoint_enable (struct arm32_watchpoint *wp)
{
  wp->enabled = 1;
}

void
arm32_watchpoint_delete (struct arm32_watchpoint_set *wps, struct arm32_watchpoint *wp)
{
  assert (wp->backidx >= 0 && wp->backidx < wps->watchpoint_count);
  assert (wps->watchpoint_list[wp->backidx] == wp);

  wps->watchpoint_list[wp->backidx] = NULL;

  if (wp->type == ARM32_WATCHPOINT_REG)
    arm32_watchpoint_set_recalc_regmask (wps);
  
  arm32_watchpoint_destroy (wp);
}

void
arm32_cpu_watchpoint_memory_pre (struct arm32_cpu *cpu, struct arm32_watchpoint *wp)
{
  struct arm32_watchpoint_set *wps = cpu->wps;
  
  if (wp->cached_phys == NULL)
    if ((wp->cached_phys = arm32_cpu_translate_read (cpu, wp->addr)) == NULL)
    {
      warning ("Watchpoint \"%s\": cannot translate address 0x%x, watchpoint disabled\n", wp->name, wp->addr);
      
      arm32_watchpoint_disable (wp);

      return;
    }

  wp->prev = *wp->cached_phys;
}

int
arm32_cpu_watchpoint_test (struct arm32_cpu *cpu, uint32_t inst, struct arm32_watchpoint *wp)
{
  int i;
  int n = 0;

  struct arm32_watchpoint_set *wps = cpu->wps;
  
  switch (wp->type)
  {
  case ARM32_WATCHPOINT_REG:
    wp->affected = 0;

    for (i = 0; i < 16; ++i)
      if (wp->mask & i)
	if (REG (cpu, i) != wps->regs_saved.r[i])
	{
	  wp->affected |= 1 << i;
	  ++n;
	}
    
    break;

  case ARM32_WATCHPOINT_MEMORY:
    if (*wp->cached_phys != wp->prev)
      ++n;
    
    break;

  case ARM32_WATCHPOINT_STEP:
    ++n;
    break;

  case ARM32_WATCHPOINT_INST:
    n += (inst & wp->mask) == (wp->inst & wp->mask);
    break;

  case ARM32_WATCHPOINT_BRANCH:
    n += PC (cpu) == cpu->next_pc;
    break;
    
  }
  
  return n;
}

struct arm32_watchpoint *
arm32_cpu_watch_branch (struct arm32_cpu *cpu, const char *name, int (*callback) (struct arm32_cpu *, struct arm32_watchpoint *, void *), void *data)
{
  struct arm32_watchpoint *wp;

  if ((wp = arm32_watchpoint_new (name, ARM32_WATCHPOINT_BRANCH, ARM32_WATCHPOINT_POST_EXEC)) == NULL)
    return NULL;

  wp->callback = callback;
  wp->data     = data;
  
  if (arm32_watchpoint_register (cpu->wps, wp) == -1)
  {
    arm32_watchpoint_destroy (wp);

    return NULL;
  }

  /* Make periodical backups of reg15 */
  cpu->wps->regmask |= 1 << 15;
  
  return wp;
}

struct arm32_watchpoint *
arm32_cpu_watch_regs (struct arm32_cpu *cpu, const char *name, int (*callback) (struct arm32_cpu *, struct arm32_watchpoint *, void *), void *data, uint16_t mask)
{
  struct arm32_watchpoint *wp;

  if ((wp = arm32_watchpoint_new (name, ARM32_WATCHPOINT_REG, ARM32_WATCHPOINT_POST_EXEC)) == NULL)
    return NULL;

  wp->callback = callback;
  wp->data     = data;
  
  wp->mask     = mask;
  
  if (arm32_watchpoint_register (cpu->wps, wp) == -1)
  {
    arm32_watchpoint_destroy (wp);

    return NULL;
  }

  return wp;
}

struct arm32_watchpoint *
arm32_cpu_watch_reg (struct arm32_cpu *cpu, const char *name, int (*callback) (struct arm32_cpu *, struct arm32_watchpoint *, void *), void *data, uint8_t reg)
{
  if (reg >= 16)
    return NULL; /* Wrong reg */

  return arm32_cpu_watch_regs (cpu, name, callback, data, 1 << reg);
}

struct arm32_watchpoint *
arm32_cpu_watch_memory (struct arm32_cpu *cpu, const char *name, int (*callback) (struct arm32_cpu *, struct arm32_watchpoint *, void *), void *data, uint32_t addr)
{
  struct arm32_watchpoint *wp;

  if ((wp = arm32_watchpoint_new (name, ARM32_WATCHPOINT_MEMORY, ARM32_WATCHPOINT_POST_EXEC)) == NULL)
    return NULL;

  wp->callback = callback;
  wp->data     = data;
  
  wp->addr     = addr;
  
  if (arm32_watchpoint_register (cpu->wps, wp) == -1)
  {
    arm32_watchpoint_destroy (wp);

    return NULL;
  }

  return wp;
}

struct arm32_watchpoint *
arm32_cpu_watch_step (struct arm32_cpu *cpu, const char *name, int (*callback) (struct arm32_cpu *, struct arm32_watchpoint *, void *), void *data)
{
  struct arm32_watchpoint *wp;

  if ((wp = arm32_watchpoint_new (name, ARM32_WATCHPOINT_STEP, ARM32_WATCHPOINT_POST_EXEC)) == NULL)
    return NULL;

  wp->callback = callback;
  wp->data     = data;
    
  if (arm32_watchpoint_register (cpu->wps, wp) == -1)
  {
    arm32_watchpoint_destroy (wp);

    return NULL;
  }

  return wp;
}

struct arm32_watchpoint *
arm32_cpu_watch_inst (struct arm32_cpu *cpu, const char *name, int (*callback) (struct arm32_cpu *, struct arm32_watchpoint *, void *), void *data, uint32_t inst, uint32_t mask)
{
  struct arm32_watchpoint *wp;

  if ((wp = arm32_watchpoint_new (name, ARM32_WATCHPOINT_INST, ARM32_WATCHPOINT_POST_EXEC)) == NULL)
    return NULL;

  wp->callback = callback;
  wp->data     = data;

  wp->inst     = inst;
  wp->mask     = mask;
  
  if (arm32_watchpoint_register (cpu->wps, wp) == -1)
  {
    arm32_watchpoint_destroy (wp);

    return NULL;
  }

  return wp;
}

int
arm32_cpu_watchpoint_set_test_pre (struct arm32_cpu *cpu, uint32_t inst)
{
  int i;
  struct arm32_watchpoint_set *set = cpu->wps;
  uint16_t regmask = set->regmask;

  if (regmask)
    for (i = 0; i < 16; ++i)
      if (regmask & (1 << i))
	set->regs_saved.r[i] = REG (cpu, i);

  for (i = 0; i < set->watchpoint_count; ++i)
    if (set->watchpoint_list[i] != NULL && set->watchpoint_list[i]->enabled)
    {
      switch (set->watchpoint_list[i]->type)
      {
      case ARM32_WATCHPOINT_MEMORY:
	arm32_cpu_watchpoint_memory_pre (cpu, set->watchpoint_list[i]);
	break;
      }

      if ((set->watchpoint_list[i]->when & ARM32_WATCHPOINT_PRE_EXEC) && arm32_cpu_watchpoint_test (cpu, inst, set->watchpoint_list[i]))
      {
	if (set->watchpoint_list[i]->callback == NULL)
	{
	  warning ("Watchpoint #%d (\"%s\") triggered, stopping execution (pre-exec)\n");
	  
	  return 1;
	}
	else if ((set->watchpoint_list[i]->callback) (cpu, set->watchpoint_list[i], set->watchpoint_list[i]->data))
	  return 1;
      }
    }
  
  return 0;
}

int
arm32_cpu_watchpoint_set_test_post (struct arm32_cpu *cpu, uint32_t inst)
{
  int i;

  struct arm32_watchpoint_set *set = cpu->wps;

  for (i = 0; i < set->watchpoint_count; ++i)
    if (set->watchpoint_list[i] != NULL && set->watchpoint_list[i]->enabled)    
      if ((set->watchpoint_list[i]->when & ARM32_WATCHPOINT_POST_EXEC) && arm32_cpu_watchpoint_test (cpu, inst, set->watchpoint_list[i]))
      {
	if (set->watchpoint_list[i]->callback == NULL)
	{
	  warning ("Watchpoint #%d (\"%s\") triggered, stopping execution (post-exec)\n");
	  
	  return 1;
	}
	else if ((set->watchpoint_list[i]->callback) (cpu, set->watchpoint_list[i], set->watchpoint_list[i]->data))
	  return 1;
      }
    
  
  return 0;
}
