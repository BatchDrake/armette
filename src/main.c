/*
 * main.c: entry point for armette
 * Creation date: Mon Sep 15 09:54:17 2014
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>

#include <armette.h>

#include <arm_cpu.h>
#include <arm_inst.h>
#include <arm_elf.h>

#include <fcntl.h>
#include <unistd.h>

int
arm32_cpu_G_CTR_Center_DecryptPacket (struct arm32_cpu *cpu, uint32_t key, char *buf, int len)
{
  struct arm32_segment *seg;
  
  if ((seg = arm32_segment_new (0x40000000, buf, __ALIGN (len, 4096), SA_R | SA_W)) == NULL)
  {
    error ("No memory to allocate segment\n");
    return -1;
  }

  if (arm32_cpu_add_segment (cpu, seg) == -1)
  {
    error ("Cannot add segment\n");

    arm32_segment_destroy (seg);
    
    return -1;
  }

  LR (cpu) = 0xeeeeeee0;

  arm32_cpu_jump (cpu, 0xc7b98);

  R0 (cpu) = key;
  R1 (cpu) = 0x40000000;
  R2 (cpu) = len;

  arm32_cpu_run (cpu);

  if (PC (cpu) != 0xeeeeeee0)
    error ("CPU failed in wrong address (PC: 0x%x)\n", PC (cpu));
  
  arm32_cpu_remove_segment (cpu, seg);

  arm32_segment_destroy (seg);

  return R0 (cpu);
}

int
main (int argc, char *argv[], char *envp[])
{
  struct arm32_cpu *cpu;
  char *my_argv[] = {"/bin/cat", "--help", NULL};
  char buf[2048];
  
  int i;
  int fd;
  int len;
  char *name;
  
  int ret;
  
  if (argc != 2)
  {
    fprintf (stderr, "Usage: %s\n", argv[0]);

    exit (EXIT_FAILURE);
  }
  
  if ((cpu = arm32_cpu_new_from_elf (argv[1])) == NULL)
  {
    fprintf (stderr, "%s: cannot create ARM32 cpu from %s: %s\n", argv[0], argv[1], strerror (errno));

    exit (EXIT_FAILURE);
  }

  for (i = 0; i < cpu->segment_count; ++i)
    if (cpu->segment_list[i] != NULL)
      printf ("%d. 0x%08x-0x%08x (%p) 0x%x\n",
	      i + 1,
	      cpu->segment_list[i]->virt,
	      cpu->segment_list[i]->virt + cpu->segment_list[i]->size,
	      cpu->segment_list[i]->phys,
	      cpu->segment_list[i]->flags);
    
  i = 0;

  do
  {
    if ((name = strbuild ("packet.%d.bin", i++)) == NULL)
    {
      fprintf (stderr, "%s: no mem!\n");

      exit (EXIT_FAILURE);
    }
    
    if ((fd = open (name, O_RDONLY)) != -1)
    {
      if ((len = read (fd, buf, 2048)) > 0)
      {
        ret = arm32_cpu_G_CTR_Center_DecryptPacket (cpu, 8, buf, len);

        printf ("Function returned %d\n", ret);
      }    

      close (fd);
    }

    free (name);
  }
  while (fd != -1);
  
  arm32_cpu_destroy (cpu);
  
  return 0;
}

