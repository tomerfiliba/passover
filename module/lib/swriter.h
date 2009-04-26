#ifndef SWRITER_H_INCLUDED
#define SWRITER_H_INCLUDED

#include <stdlib.h>
#include <stdint.h>

#include "errors.h"


typedef struct _swriter_t {
	void * buffer;
	size_t size;
	void * pos;
} swriter_t;


errcode_t swriter_init(swriter_t * self, size_t size);
errcode_t swriter_fini(swriter_t * self);
errcode_t swriter_dump_buffer(swriter_t * self, const void * buf, size_t size);
errcode_t swriter_dump_uint8(swriter_t * self, uint8_t value);
errcode_t swriter_dump_uint16(swriter_t * self, uint16_t value);
errcode_t swriter_dump_uint32(swriter_t * self, uint32_t value);
errcode_t swriter_dump_uint64(swriter_t * self, uint64_t value);
errcode_t swriter_dump_pstr(swriter_t * self, const char * value, size_t length);
errcode_t swriter_dump_cstr(swriter_t * self, const char * value);
size_t swriter_get_length(swriter_t * self);
void * swriter_get_buffer(swriter_t * self);
errcode_t swriter_copy_into(swriter_t * self, void * buffer, size_t size);
errcode_t swriter_clear(swriter_t * self);

#endif
