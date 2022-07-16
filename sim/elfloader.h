#ifndef ELFLOADER_H
#define ELFLOADER_H

#include <sys/stat.h>

typedef struct elf_t {
  unsigned entry_address;
  char *head;
  struct stat file_stat;
  unsigned programs;
  unsigned *program_size;
  unsigned *program_base;
  char **program;
} elf_t;

void elf_init(elf_t *, const char *elf_path);
void elf_fini(elf_t *);

#endif
