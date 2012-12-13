#ifndef MAPMEM_H_
#define MAPMEM_H_

#include <stdint.h>

typedef struct {
  uint8_t* mem;
  uint16_t size;
  uint16_t index;
} mapmem_t;

int mapmem_init(mapmem_t* mapmem, uint16_t size);
void mapmem_delete(mapmem_t* mapmem);

uint8_t* mapmem_allocate(mapmem_t* mapmem, uint16_t size);
uint8_t* mapmem_copy_data(mapmem_t* mapmem, void* data, uint16_t len);
uint8_t* mapmem_copy_text(mapmem_t* mapmem, char* text);

#endif /* MAPMEM_H_ */
