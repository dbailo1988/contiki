#include <stdlib.h>
#include <string.h>

#include "mapmem.h"

int
mapmem_init(mapmem_t* mapmem, uint16_t size)
{
  mapmem_delete(mapmem);
  mapmem->mem = (uint8_t*)malloc(size);
  if (mapmem->mem) {
	memset(mapmem->mem, 0, size);
    mapmem->size = size;
    mapmem->index = 0;
    return 1;
  }

  return 0;
}

void
mapmem_delete(mapmem_t* mapmem)
{
  if (mapmem->mem) {
    free(mapmem->mem);
    mapmem->mem = NULL;
    mapmem->index = 0;
    mapmem->size = 0;
  }
}

uint8_t*
mapmem_allocate(mapmem_t* mapmem, uint16_t size)
{
  int rem = 0;
  /*To get rid of alignment problems, always allocate even size*/
  rem = size % 4;
  if (rem) {
    size += (4-rem);
  }

  uint8_t* block = NULL;
  if (mapmem->index + size < mapmem->size) {
    block = mapmem->mem + mapmem->index;
    mapmem->index += size;
  }

  return block;
}

uint8_t*
mapmem_copy_data(mapmem_t* mapmem, void* data, uint16_t len)
{
  uint8_t* block = mapmem_allocate(mapmem, len);
  if (block) {
    memcpy(block, data, len);
  }

  return block;
}

uint8_t*
mapmem_copy_text(mapmem_t* mapmem, char* text)
{
  uint8_t* block = mapmem_allocate(mapmem, strlen(text) + 1);
  if (block) {
    strcpy((char *)block, text);
  }

  return block;
}

