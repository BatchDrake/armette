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


#include <stdarg.h>
#include <string.h>
#include <sys/mman.h>

#include "arm_cpu.h"
#include "arm_watch.h"

struct arm32_cpu *curr_cpu;

unsigned int debuglevel = ARMETTE_ERROR;
uint32_t arm_vdso[] = {ARM32_ARMETTE_RETURN_INSTRUCTION};

void
arm32_dbg (unsigned int level, const char *fmt, ...)
{
  char *msg;
  va_list ap;
  static char *errpfx = " E!id";
  static char last_c = '\n';
  
  if (level <= debuglevel && debuglevel <= ARMETTE_DEBUG)
  {
    va_start (ap, fmt);
    
    if ((msg = vstrbuild (fmt, ap)) != NULL)
    {
      if (last_c == '\n')
        fprintf (stderr, "[%c:%08x] ", errpfx[level], curr_cpu ? PC (curr_cpu) - 8: -1);

      fprintf (stderr, "%s", msg);

      last_c = msg[strlen (msg) - 1];

      free (msg);
    }

    va_end (ap);
  }
}

static void
__arm32_segment_mmap_dtor (void *data, void *phys, uint32_t size)
{
  munmap (phys, size);
}

int
arm32_cpu_add_stack (struct arm32_cpu *cpu)
{
  struct arm32_segment *seg;
  void *stack_base;
  
  if ((stack_base = mmap (NULL, ARM32_DEFAULT_STACK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0)) == (caddr_t) -1)
    return -1;
  
  if ((seg = arm32_segment_new (ARM32_DEFAULT_STACK_BOTTOM - ARM32_DEFAULT_STACK_SIZE, stack_base, ARM32_DEFAULT_STACK_SIZE, SA_R | SA_W)) == NULL)
  {
    munmap (stack_base, ARM32_DEFAULT_STACK_SIZE);
    
    return -1;
  }

  arm32_segment_set_dtor (seg, __arm32_segment_mmap_dtor, NULL);
  
  if (arm32_cpu_add_segment (cpu, seg) == -1)
  {
    arm32_segment_destroy (seg);

    return -1;
  }

  SP (cpu) = ARM32_DEFAULT_STACK_BOTTOM - 4;

  return 0;
}

int
arm32_cpu_add_armette_vdso (struct arm32_cpu *cpu)
{
  struct arm32_segment *seg;

  if ((seg = arm32_segment_new (ARM32_DEFAULT_VDSO_BOTTOM, arm_vdso, sizeof (arm_vdso), SA_R | SA_X)) == NULL)
    return -1;

  if (arm32_cpu_add_segment (cpu, seg) == -1)
  {
    arm32_segment_destroy (seg);

    return -1;
  }

  return 0;
}

struct arm32_cpu *
arm32_cpu_new (void)
{
  struct arm32_cpu *new;

  if ((new = calloc (1, sizeof (struct arm32_cpu))) == NULL)
    return NULL;

  if (arm32_cpu_add_stack (new) == -1)
    goto fail;
  
  if (arm32_cpu_add_armette_vdso (new) == -1)
    goto fail;

  if ((new->wps = arm32_watchpoint_set_new ()) == NULL)
    goto fail;
  
  return new;

fail:
  arm32_cpu_destroy (new);

  return NULL;
}

uint32_t
arm32_cpu_find_region (const struct arm32_cpu *cpu, uint32_t size, uint32_t align)
{
  int i;

  uint32_t guess = 0x1000; /* It's a good idea to avoid 0x0 as many programs consider it a wrong location */
  uint32_t new;
  
  size = __ALIGN (size, align);
  
  do
  {
    for (i = 0; i < cpu->segment_count; ++i)
      if (cpu->segment_list[i] != NULL)
        if ((cpu->segment_list[i]->virt <= guess &&
             guess < cpu->segment_list[i]->virt + cpu->segment_list[i]->size) ||
            (cpu->segment_list[i]->virt <= guess + size - 1 &&
             guess + size - 1 < cpu->segment_list[i]->virt + cpu->segment_list[i]->size))
          break;
    
    if (i < cpu->segment_count)
    {
      new = __ALIGN (cpu->segment_list[i]->virt + cpu->segment_list[i]->size, align);

      if (new < guess || new + size < guess)
        return -1;

      guess = new;
    }
  }
  while (i < cpu->segment_count);
  
  return guess;
}

uint32_t
arm32_map_rw_buffer (struct arm32_cpu *cpu, void *data, size_t size)
{
  struct arm32_segment *seg;
  uint32_t addr;

  if ((addr = arm32_cpu_find_region (cpu, size, 16)) == -1)
    return -1;
  
  if ((seg = arm32_segment_new (addr, data, size, SA_R | SA_W)) == NULL)
    return -1;
  
  if (arm32_cpu_add_segment (cpu, seg) == -1)
    return -1;

  return addr;
}

uint32_t
arm32_map_ro_buffer (struct arm32_cpu *cpu, void *data, size_t size)
{
  struct arm32_segment *seg;
  uint32_t addr;

  if ((addr = arm32_cpu_find_region (cpu, size, 16)) == -1)
    return -1;
  
  if ((seg = arm32_segment_new (addr, data, size, SA_R)) == NULL)
    return -1;
  
  if (arm32_cpu_add_segment (cpu, seg) == -1)
    return -1;

  return addr;
}

uint32_t
arm32_map_exec_buffer (struct arm32_cpu *cpu, void *data, size_t size)
{
  struct arm32_segment *seg;
  uint32_t addr;

  if ((addr = arm32_cpu_find_region (cpu, size, 16)) == -1)
    return -1;
  
  if ((seg = arm32_segment_new (addr, data, size, SA_R | SA_X)) == NULL)
    return -1;
  
  if (arm32_cpu_add_segment (cpu, seg) == -1)
    return -1;

  return addr;
}

struct arm32_segment *
arm32_segment_new (uint32_t virt, void *phys, uint32_t size, uint8_t flags)
{
  struct arm32_segment *new;

  if ((new = malloc (sizeof (struct arm32_segment))) == NULL)
    return NULL;

  new->virt  = virt;
  new->size  = size;
  new->phys  = phys;
  new->dtor  = NULL;
  new->flags = flags;
  
  return new;
}

void
arm32_segment_set_dtor (struct arm32_segment *seg, void (*dtor) (void *, void *, uint32_t), void *data)
{
  seg->dtor = dtor;
  seg->data = data;
}

void
arm32_cpu_set_dtor (struct arm32_cpu *cpu, void (*dtor) (void *), void *data)
{
  cpu->dtor = dtor;
  cpu->data = data;
}

int
arm32_cpu_add_segment (struct arm32_cpu *cpu, struct arm32_segment *seg)
{
  if (seg == NULL)
    return -1;

  return PTR_LIST_APPEND_CHECK (cpu->segment, seg);
}

int
arm32_cpu_remove_segment (struct arm32_cpu *cpu, struct arm32_segment *seg)
{
  int i;

  for (i = 0; i < cpu->segment_count; ++i)
    if (cpu->segment_list[i] == seg)
    {
      cpu->segment_list[i] = NULL;
      return i;
    }

  return -1;
}

void
arm32_segment_destroy (struct arm32_segment *seg)
{
  if (seg->dtor != NULL)
    (seg->dtor) (seg->data, seg->phys, seg->size);

  free (seg);
}

void
arm32_cpu_destroy (struct arm32_cpu *cpu)
{
  int i;

  for (i = 0; i < cpu->segment_count; ++i)
    if (cpu->segment_list[i] != NULL)
      arm32_segment_destroy (cpu->segment_list[i]);

  if (cpu->segment_list != NULL)
    free (cpu->segment_list);

  if (cpu->dtor != NULL)
    (cpu->dtor) (cpu->data);

  if (cpu->wps != NULL)
    arm32_watchpoint_set_destroy (cpu->wps);
  
  free (cpu);
}

