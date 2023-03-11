#include "elfloader.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>

#define STRINGIZE(x) STRINGIZE2(x)
#define STRINGIZE2(x) #x
#define FILE_LINE(n) (__FILE__ " L" STRINGIZE(__LINE__) " " n)

#define EI_NIDENT 16
typedef uint32_t Elf32_Word;
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Addr;
typedef uint32_t Elf32_Off;

typedef struct {
  unsigned char e_ident[EI_NIDENT]; // magic number; I like sakamoto maaya.
  Elf32_Half e_type;
  Elf32_Half e_machine;
  Elf32_Word e_version;
  Elf32_Addr e_entry;
  Elf32_Off e_phoff;
  Elf32_Off e_shoff;
  Elf32_Word e_flags;
  Elf32_Half e_ehsize;
  Elf32_Half e_phentsize;
  Elf32_Half e_phnum;
  Elf32_Half e_shentsize;
  Elf32_Half e_shnum;
  Elf32_Half e_shstrndx;
} Elf32_Ehdr;

#define PT_LOAD 1

typedef struct {
  Elf32_Word p_type;
  Elf32_Off p_offset;
  Elf32_Addr p_vaddr;
  Elf32_Addr p_paddr;
  Elf32_Word p_filesz;
  Elf32_Word p_memsz;
  Elf32_Word p_flags;
  Elf32_Word p_align;
} Elf32_Phdr;

void elf_init(elf_t *elf, const char *elf_path) {
  FILE *fp = NULL;
  Elf32_Ehdr *elf_header;
  elf->status = ELF_STATUS_UNLOADED;
  // read headers
  if ((fp = fopen(elf_path, "r")) == NULL) {
    perror(FILE_LINE("fopen"));
    goto cleanup;
  }
  fstat(fileno(fp), &elf->file_stat);
  elf->head = (char *)mmap(NULL, elf->file_stat.st_size, PROT_READ, MAP_PRIVATE, fileno(fp), 0);
  if (elf->head == MAP_FAILED) {
    perror(FILE_LINE("mmap"));
    goto cleanup;
  }
  // elf header
  elf_header = (Elf32_Ehdr *)elf->head;
  // set program entry address
  elf->entry_address = elf_header->e_entry;

  // set programs
  elf->programs = 0;
  elf->program_file_size = NULL;
  elf->program_mem_size = NULL;
  elf->program = NULL;
  elf->program_base = NULL;
  for (int i = 0; i < elf_header->e_phnum; i++) {
    Elf32_Phdr *ph = (Elf32_Phdr *)(&elf->head[elf_header->e_phoff + elf_header->e_phentsize * i]);
    switch (ph->p_type) {
    case PT_LOAD:
      elf->program_file_size = (unsigned *)realloc(elf->program_file_size, (elf->programs + 1) * sizeof(unsigned));
      elf->program_mem_size = (unsigned *)realloc(elf->program_mem_size, (elf->programs + 1) * sizeof(unsigned));
      elf->program_file_size[elf->programs] = ph->p_filesz;
      elf->program_mem_size[elf->programs] = ph->p_memsz;
      elf->program_base = (unsigned *)realloc(elf->program_base, (elf->programs + 1) * sizeof(unsigned));
      elf->program_base[elf->programs] = ph->p_paddr;
      elf->program = (char **)realloc(elf->program, (elf->programs + 1) * sizeof(char *));
      elf->program[elf->programs] = &(elf->head[ph->p_offset]);
      elf->programs++;
#if 0
      fprintf(stderr, "LOAD: p_paddr %08x memsize %08x filesize %08x\n", ph->p_paddr, ph->p_memsz, ph->p_filesz);
#endif
      break;
    default:
#if 0
      fprintf(stderr, "NOT LOAD: p_paddr %08x memsize %08x filesize %08x\n", ph->p_paddr, ph->p_memsz, ph->p_filesz);
#endif
      break;
    }
  }
  elf->status = ELF_STATUS_LOADED;
 cleanup:
  if (fp) {
    fclose(fp);
  }
  return;
}

void elf_fini(elf_t *elf) {
  munmap(elf->head, elf->file_stat.st_size);
  free(elf->program);
  free(elf->program_file_size);
  free(elf->program_mem_size);
  free(elf->program_base);
  return;
}
