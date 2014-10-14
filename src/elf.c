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

#include <string.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include <arm_cpu.h>
#include <arm_inst.h>
#include <arm_elf.h>

uint32_t arm_errno_virt;
uint32_t *arm_errno;

static int
arm32_elf_is_sane (const struct arm32_elf *elf)
{
  if (memcmp (elf->ehdr->e_ident, "\x7f" "ELF", 4))
    return 0;

  /* Currently only 32 bit supported */
  if (elf->ehdr->e_ident[EI_CLASS] != ELFCLASS32)
    return 0;

  /* Currently only little endian supported */
  if (elf->ehdr->e_ident[EI_DATA]  != ELFDATA2LSB)
    return 0;

  if (elf->ehdr->e_type != ET_EXEC && elf->ehdr->e_type != ET_DYN)
    return 0;

  /* Not an ARM executable */
  if (elf->ehdr->e_machine != EM_ARM)
    return 0;

  /* Incompatible Phdr size */
  if (elf->ehdr->e_phentsize != sizeof (Elf32_Phdr))
    return 0;

  /* Program header list too large */
  if (elf->ehdr->e_phoff + elf->ehdr->e_phnum * elf->ehdr->e_phentsize > elf->size)
    return 0;
  
  return 1;
}

static int
arm32_elf_phdr_is_sane (const struct arm32_elf *elf, const Elf32_Phdr *phdr)
{
  return phdr->p_offset + phdr->p_filesz <= elf->size;    
}

void
arm32_elf_destroy (struct arm32_elf *elf)
{
  int i;

  for (i = 0; i < elf->override_count; ++i)
    if (elf->override_list[i] != NULL)
    {
      if (elf->override_list[i]->name != NULL)
        free (elf->override_list[i]->name);
      
      free (elf->override_list[i]);
    }

  if (elf->override_list != NULL)
    free (elf->override_list);
  
  if (elf->base != NULL && elf->base != (caddr_t) -1)
    munmap (elf->base, elf->size);

  if (elf->stack_base != NULL && elf->stack_base != (caddr_t) -1)
    munmap (elf->stack_base, elf->stack_size);
  
  free (elf);
}

static void
arm32_elf_dtor (void *data)
{
  arm32_elf_destroy ((struct arm32_elf *) data);
}

void
arm32_elf_segment_dtor (void *data, void *phys, uint32_t size)
{
  struct arm32_elf *elf;

  elf = (struct arm32_elf *) data;

  /* Segment is bigger in memory */
  if (phys < elf->base || (elf->base + elf->size) <= phys)
    munmap (phys, size);
}

void *
arm32_elf_translate (struct arm32_elf *elf, uint32_t virt)
{
  int i;

  for (i = 0; i < elf->ehdr->e_phnum; ++i)
    if (elf->phdr[i].p_type == PT_LOAD)
      if (elf->phdr[i].p_vaddr <= virt && virt < elf->phdr[i].p_vaddr + elf->phdr[i].p_filesz && elf->phdr[i].p_offset + elf->phdr[i].p_filesz <= elf->size)
        return virt - elf->phdr[i].p_vaddr + elf->base + elf->phdr[i].p_offset;

  return NULL;
}

void
arm32_elf_dynamic_init (struct arm32_elf *elf)
{
  int i, j;
  int dynamic_entries;
  
  Elf32_Phdr *dynamic = NULL;
  Elf32_Dyn *dyn;

  uint32_t *hash;
  uint32_t strtab_virt;
  
  for (i = 0; i < elf->ehdr->e_phnum; ++i)
    if (elf->phdr[i].p_type == PT_DYNAMIC)
    {
      dynamic = &elf->phdr[i];
      break;
    }

  /* Static executable */
  if (dynamic == NULL)
    return;
  
  if (dynamic->p_filesz + dynamic->p_offset > elf->size)
  {
    error ("arm32_elf_dynamic_init: broken ELF\n");
    return;
  }
    
  dynamic_entries = dynamic->p_filesz / sizeof (Elf32_Dyn);
  dyn = (Elf32_Dyn *) (elf->base + dynamic->p_offset);
  
  for (i = 0; i < dynamic_entries; ++i)
  {
    switch (dyn[i].d_tag)
    {
    case DT_PLTGOT:
      if ((elf->got = (uint32_t *) arm32_elf_translate (elf, dyn[i].d_un.d_ptr)) == NULL)
        error ("arm32_elf_dynamic_init: cannot translate DT_PLTGOT address (0x%x)\n", dyn[i].d_un.d_ptr);
      
      break;

    case DT_SYMTAB:
      if ((elf->symtab = (Elf32_Sym *) arm32_elf_translate (elf, dyn[i].d_un.d_ptr)) == NULL)
        error ("arm32_elf_dynamic_init: cannot translate DT_SYMTAB address (0x%x)\n", dyn[i].d_un.d_ptr);
      
      break;

    case DT_HASH:
      if ((hash = (uint32_t *) arm32_elf_translate (elf, dyn[i].d_un.d_ptr)) != NULL)
        elf->symtab_size = hash[1];
      else
        error ("arm32_elf_dynamic_init: cannot translate DT_HASH address (0x%x)\n", dyn[i].d_un.d_ptr);
            
      break;

    case DT_GNU_HASH:
      if ((hash = (uint32_t *) arm32_elf_translate (elf, dyn[i].d_un.d_ptr)) != NULL)
      {
	/* Extremely dangerous. Check whether traversing hash[0] entries is safe */
        elf->symtab_size = 0;
	elf->symtab_first = hash[1] - 1;
	for (j = 0; j < hash[0]; ++j)
	  if (elf->symtab_size < hash[4 + hash[2] + j])
	    elf->symtab_size = hash[4 + hash[2] + j];

	++elf->symtab_size;
      }
      else
        error ("arm32_elf_dynamic_init: cannot translate DT_GNU_HASH address (0x%x)\n", dyn[i].d_un.d_ptr);
            
      break;

    case DT_STRSZ:
      elf->strtab_size = dyn[i].d_un.d_val;
      
      break;
      
    case DT_STRTAB:
      if ((elf->strtab = (char *) arm32_elf_translate (elf, strtab_virt = dyn[i].d_un.d_ptr)) == NULL)
        error ("arm32_elf_dynamic_init: cannot translate DT_STRTAB address (0x%x)\n", dyn[i].d_un.d_ptr);

      break;
    }
  }

  
  if (elf->got != NULL && elf->symtab != NULL && elf->symtab_size > 0 && elf->strtab != NULL && elf->strtab_size > 0)
  {
    /* Check whether the last byte of strtab is accesible */
    if (arm32_elf_translate (elf, strtab_virt + elf->strtab_size - 1) == NULL)
      error ("Symtab: broken strtab!\n");
    else
      elf->symtab_sane = 1;
  }
}

int
arm32_elf_dummy_import (struct arm32_cpu *cpu, const char *name, void *data, uint32_t prev)
{
  error ("Undefined function `%s()'\n", name);
  
  EXCEPT (ARM32_EXCEPTION_UNDEF);
}


void
arm32_elf_fix_imports (struct arm32_elf *elf)
{
  int i;

  if (elf->symtab_sane)
    for (i = elf->symtab_first; i < elf->symtab_size; ++i)
      if (elf->symtab[i].st_name < elf->strtab_size)
	arm32_cpu_define_symbol (elf, elf->strtab + elf->symtab[i].st_name, i, arm32_elf_dummy_import, NULL);
}

void
arm32_elf_init_debug_symbols (struct arm32_elf *elf)
{
  int i;
  int maxnameoff;
  Elf32_Shdr *shdrs;
  
  if (elf->ehdr->e_shnum > 0 && (elf->ehdr->e_shoff + elf->ehdr->e_shnum * sizeof (Elf32_Shdr)) <= elf->size && elf->ehdr->e_shstrndx < elf->ehdr->e_shnum)
  {
    shdrs = (Elf32_Shdr *) (elf->base + elf->ehdr->e_shoff);

    if (shdrs[elf->ehdr->e_shstrndx].sh_offset + (maxnameoff = shdrs[elf->ehdr->e_shstrndx].sh_size) <= elf->size)
      for (i = 0; i < elf->ehdr->e_shnum; ++i)
        if (shdrs[i].sh_name < maxnameoff)
          if (shdrs[i].sh_type == SHT_SYMTAB &&
              shdrs[i].sh_offset + shdrs[i].sh_size <= elf->size &&
              shdrs[i].sh_link < elf->ehdr->e_shnum &&
              shdrs[shdrs[i].sh_link].sh_type == SHT_STRTAB && /* I definitely love sequence points */
              shdrs[shdrs[i].sh_link].sh_offset + shdrs[shdrs[i].sh_link].sh_size <= elf->size
            ) 
          {
            elf->debug_symtab = (Elf32_Sym *) (elf->base + shdrs[i].sh_offset);
            elf->debug_strtab =      (char *) (elf->base + shdrs[shdrs[i].sh_link].sh_offset);
            
            elf->debug_symtab_size = shdrs[i].sh_size / sizeof (Elf32_Sym);
            elf->debug_strtab_size = shdrs[shdrs[i].sh_link].sh_size;
            
            return;
          }
    
  }
}

uint32_t
arm32_elf_resolve_debug_symbol (struct arm32_elf *elf, const char *name)
{
  int i;

  for (i = 0; i < elf->debug_symtab_size; ++i)
    if (elf->debug_symtab[i].st_name < elf->debug_strtab_size)
      if (strcmp (elf->debug_strtab + elf->debug_symtab[i].st_name, name) == 0)
        return elf->debug_symtab[i].st_value;

  return 0;
}


struct arm32_cpu *
arm32_cpu_new_from_elf (const char *path)
{
  struct arm32_cpu *new = NULL;
  struct arm32_segment *seg;
  
  void *base;
  void *seg_base;

  size_t size;
  size_t seg_size;

  uint8_t seg_flags;
  
  int fd;
  int i;
  
  struct arm32_elf *elf = NULL; 
  
  if ((fd = open (path, O_RDONLY)) == -1)
    return NULL;

  size = lseek (fd, 0, SEEK_END);

  if (size < sizeof (Elf32_Ehdr))
  {
    close (fd);
    
    errno = ENOEXEC;

    return NULL;
  }

  base = mmap (NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
  
  close (fd);

  if (base == (caddr_t) -1)
    return NULL;

  if ((elf = calloc (1, sizeof (struct arm32_elf))) == NULL)
  {
    munmap (base, size);

    return NULL;
  }

  elf->base = base;
  elf->size = size;

  elf->ehdr = (Elf32_Ehdr *) base;
  elf->phdr = (Elf32_Phdr *) (base + elf->ehdr->e_phoff);

  if (!arm32_elf_is_sane (elf))
  {
    errno = ENOEXEC;
    
    goto fail;
  }
  
  if ((new = arm32_cpu_new ()) == NULL)
    goto fail;

  arm32_cpu_set_dtor (new, arm32_elf_dtor, elf);
  
  for (i = 0; i < elf->ehdr->e_phnum; ++i)
  {
    if (elf->phdr[i].p_type == PT_LOAD)
    {
      if (!arm32_elf_phdr_is_sane (elf, &elf->phdr[i]))
      {
	errno = ENOEXEC;
	
	goto fail;
      }
      
      seg_size = elf->phdr[i].p_memsz;

      seg_flags = 0;

      if (elf->phdr[i].p_flags & PF_X)
	seg_flags |= SA_X;

      if (elf->phdr[i].p_flags & PF_W)
	seg_flags |= SA_W;

      if (elf->phdr[i].p_flags & PF_R)
	seg_flags |= SA_R;
      
      if (seg_size != elf->phdr[i].p_filesz)
      {
	if ((seg_base = mmap (NULL, seg_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0)) == (caddr_t) -1)
	  goto fail;

	memcpy (seg_base, base + elf->phdr[i].p_offset, elf->phdr[i].p_filesz);
      }
      else
	seg_base = base + elf->phdr[i].p_offset;
      
      if ((seg = arm32_segment_new (elf->phdr[i].p_vaddr, seg_base, seg_size, seg_flags)) == NULL)
      {
	if (seg_size != elf->phdr[i].p_filesz)
	  munmap (seg_base, seg_size);

	goto fail;
      }

      arm32_segment_set_dtor (seg, arm32_elf_segment_dtor, elf);

      if (arm32_cpu_add_segment (new, seg) == -1)
      {
	arm32_segment_destroy (seg);

	goto fail;
      }
    }
  }

  new->next_pc = elf->ehdr->e_entry;
  
  arm32_cpu_jump (new, elf->ehdr->e_entry);

  arm32_elf_dynamic_init (elf);

  arm32_elf_fix_imports (elf);

  arm32_elf_init_debug_symbols (elf);
  
  return new;
  
fail:
  if (new != NULL)
    arm32_cpu_destroy (new);
  else if (elf != NULL)
    arm32_elf_destroy (elf);

  return NULL;
}

int
arm32_cpu_get_symbol_index (struct arm32_cpu *cpu, const char *name)
{
  int i;

  struct arm32_elf *elf = (struct arm32_elf *) cpu->data;

  if (!elf->symtab_sane)
    return -1;
  
  for (i = elf->symtab_first; i < elf->symtab_size; ++i)
    if (elf->symtab[i].st_name < elf->strtab_size)
      if (strcmp (name, elf->strtab + elf->symtab[i].st_name) == 0)
	return i;

  return -1;
}

void
arm32_elf_remove_symbol (struct arm32_elf *elf, const char *name)
{
  int i;

  for (i = 0; i < elf->override_count; ++i)
    if (strcmp (elf->override_list[i]->name, name) == 0)
    {
      free (elf->override_list[i]->name);
      free (elf->override_list[i]);
      elf->override_list[i] = NULL;
    }
}

int
arm32_cpu_override_symbol (struct arm32_cpu *cpu, const char *name, int (*callback) (struct arm32_cpu *, const char *name, void *data, uint32_t), void *data)
{
  int i;
  struct arm32_elf *elf = (struct arm32_elf *) cpu->data;
  
  for (i = 0; i < elf->override_count; ++i)
    if (elf->override_list[i] != NULL && elf->override_list[i]->name != NULL)
      if (strcmp (elf->override_list[i]->name, name) == 0)
      {
	elf->override_list[i]->callback = callback;
	elf->override_list[i]->data = data;

	return 0;
      }

  return -1;
}

int
arm32_cpu_restore_symbol (struct arm32_cpu *cpu, const char *name)
{
  int i;
  struct arm32_elf *elf = (struct arm32_elf *) cpu->data;
  
  for (i = 0; i < elf->override_count; ++i)
    if (elf->override_list[i] != NULL && elf->override_list[i]->name != NULL)
      if (strcmp (elf->override_list[i]->name, name) == 0)
      {
	*elf->override_list[i]->phys = elf->override_list[i]->prev;

        
	return 0;
      }

  return -1;
}

int
arm32_elf_replace_instruction (struct arm32_elf *elf, const char *name, uint32_t vaddr, int (*callback) (struct arm32_cpu *, const char *name, void *data, uint32_t), void *data)
{
  struct arm32_elf_instruction_override *new;
  uint32_t *addr;
  int sym_idx;
  
  if ((addr = arm32_elf_translate (elf, vaddr)) == NULL)
    return 1;
  
  if ((new = malloc (sizeof (struct arm32_elf_instruction_override))) == NULL)
    return -1;

  if (name == NULL)
    new->name = NULL;
  else
    if ((new->name = strdup (name)) == NULL)
    {
      free (new);
      return -1;
    }
  
  new->callback = callback;
  new->data = data;

  if ((sym_idx = PTR_LIST_APPEND_CHECK (elf->override, new)) == -1)
  {
    free (new->name);
    free (new);

    return -1;
  }

  new->phys = addr;
  new->prev = *addr; /* Save old instruction */
  
  *addr = 0xef000000 + ((sym_idx + ARM32_IMPORT_HOOK_BASE) & 0xffffff);

  return 0;
}

int
arm32_cpu_define_symbol (struct arm32_elf *elf, const char *name, int sym_idx, int (*callback) (struct arm32_cpu *, const char *, void *, uint32_t), void *data)
{
  return arm32_elf_replace_instruction (elf, name, elf->symtab[sym_idx].st_value, callback, NULL);
}

int
arm32_elf_call_external (struct arm32_cpu *cpu, uint32_t sym)
{
  struct arm32_elf *elf = (struct arm32_elf *) cpu->data;
  int ret;
  
  /* Regular SWI interrupt */
  if (sym < ARM32_IMPORT_HOOK_BASE || (sym - ARM32_IMPORT_HOOK_BASE > elf->override_count))
    EXCEPT (ARM32_EXCEPTION_SWI);

  if (elf->override_list[sym - ARM32_IMPORT_HOOK_BASE] == NULL)
    EXCEPT (ARM32_EXCEPTION_UNDEF);
  else
  {
    debug ("  Call overriden %s()\n", elf->override_list[sym - ARM32_IMPORT_HOOK_BASE]->name == NULL ? "<unknown>" : elf->override_list[sym - ARM32_IMPORT_HOOK_BASE]->name);

    if ((ret = (elf->override_list[sym - ARM32_IMPORT_HOOK_BASE]->callback)
         (
           cpu,
           elf->override_list[sym - ARM32_IMPORT_HOOK_BASE]->name,
           elf->override_list[sym - ARM32_IMPORT_HOOK_BASE]->data,
           elf->override_list[sym - ARM32_IMPORT_HOOK_BASE]->prev)
          ) < 0)
      return ret;
  }
  return ret;
}

int
arm32_cpu_prepare_main (struct arm32_cpu *cpu, int argc, char **argv)
{
  uint8_t *main_context;
  uint32_t *virt_argv;
  int i;
  int p = 0;
  struct arm32_segment *seg;
  uint32_t required_len = 12;
  
  /* TODO: predict the size of the whole array, this is ok by now, but not in general */

  for (i = 0; i < argc; ++i)
    required_len += strlen (argv[i]) + 1 + sizeof (uint32_t);
  
  if ((main_context = mmap (NULL, required_len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0)) == (uint8_t *) -1)
    return -1;

  virt_argv = (uint32_t *) main_context;

  /* Value in the top of the stack corresponds to argc */
  virt_argv[0] = argc;

  for (i = 0; i < argc; ++i)
  {
    memcpy (&main_context[sizeof (uint32_t) * (argc + 2) + p],
	    argv[i],
	    strlen (argv[i]) + 1);

    virt_argv[i + 1] = ARM32_DEFAULT_STACK_BOTTOM + sizeof (uint32_t) * (argc + 2) + p;

    debug ("Put argument \"%s\" --> 0x%x\n", argv[i], virt_argv[i + 1]);
    
    p += strlen (argv[i]) + 1;
  }
  
  /* Argv ends in NULL */
  virt_argv[argc + 1] = 0;

  /* ARM errno goes here */
  arm_errno = &virt_argv[argc + 2 + __UNITS (p, sizeof (uint32_t))];  
  arm_errno_virt = ARM32_DEFAULT_STACK_BOTTOM +(sizeof (uint32_t) * (argc + 2 + __UNITS (p, sizeof (uint32_t))));

  *arm_errno = 0;
						
  if ((seg = arm32_segment_new (ARM32_DEFAULT_STACK_BOTTOM, main_context, __ALIGN (required_len, 4096), SA_R | SA_W)) == NULL)
  {
    munmap (main_context, required_len);

    return -1;
  }

  arm32_segment_set_dtor (seg, arm32_elf_segment_dtor, cpu->data);
  
  if (arm32_cpu_add_segment (cpu, seg) == -1)
  {
    /* Munmap happens implicitly */
    arm32_segment_destroy (seg);

    return -1;
  }

  /* SP points to the stack of _start */
  
  SP (cpu) = ARM32_DEFAULT_STACK_BOTTOM;
  
  return 0;
}

