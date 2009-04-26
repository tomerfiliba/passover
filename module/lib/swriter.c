#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <malloc.h>

#include "swriter.h"


errcode_t swriter_init(swriter_t * self, size_t size)
{
	self->size = size;
	self->buffer = malloc(size);
	if (self->buffer == NULL) {
		return ERR_SWRITER_MALLOC_FAILED;
	}
	self->pos = self->buffer;
	RETURN_SUCCESSFUL;
}

errcode_t swriter_fini(swriter_t * self)
{
	if (self->buffer != NULL) {
		free(self->buffer);
		self->buffer = NULL;
	}
	RETURN_SUCCESSFUL;
}

inline errcode_t swriter_dump_buffer(swriter_t * self, const void * buf, size_t size)
{
	if (size > ((self->buffer + self->size) - self->pos)) {
		return ERR_SWRITER_DUMP_TOO_BIG;
	}
	memcpy(self->pos, buf, size);
	self->pos += size;
	RETURN_SUCCESSFUL;
}

errcode_t swriter_dump_uint8(swriter_t * self, uint8_t value)
{
	return swriter_dump_buffer(self, &value, sizeof(value));
}

errcode_t swriter_dump_uint16(swriter_t * self, uint16_t value)
{
	return swriter_dump_buffer(self, &value, sizeof(value));
}

errcode_t swriter_dump_uint32(swriter_t * self, uint32_t value)
{
	return swriter_dump_buffer(self, &value, sizeof(value));
}

errcode_t swriter_dump_uint64(swriter_t * self, uint64_t value)
{
	return swriter_dump_buffer(self, &value, sizeof(value));
}

inline errcode_t swriter_dump_pstr(swriter_t * self, const char * value, size_t length)
{
	if (length > 65535) {
		length = 65535;
	}
	PROPAGATE(swriter_dump_uint16(self, length));
	errcode_t retcode = swriter_dump_buffer(self, value, length);
	if (IS_ERROR(retcode)) {
		self->pos -= sizeof(length); // undo: remove length field
		return retcode;
	}

	RETURN_SUCCESSFUL;
}

errcode_t swriter_dump_cstr(swriter_t * self, const char * value)
{
	return swriter_dump_pstr(self, value, strlen(value));
}

inline size_t swriter_get_length(swriter_t * self)
{
	return (size_t)(self->pos - self->buffer);
}

void * swriter_get_buffer(swriter_t * self)
{
	return self->buffer;
}

errcode_t swriter_copy_into(swriter_t * self, void * buffer, size_t size)
{
	size_t length = swriter_get_length(self);
	if (length > size) {
		return ERR_SWRITER_COPY_DEST_BUF_TOO_SMALL;
	}
	memcpy(buffer, self->buffer, length);
	RETURN_SUCCESSFUL;
}

errcode_t swriter_clear(swriter_t * self)
{
	self->pos = self->buffer;
	RETURN_SUCCESSFUL;
}


