#ifndef LISTFILE_H_INCLUDED
#define LISTFILE_H_INCLUDED

#include <stdlib.h>
#include <stdint.h>

#include "fmap.h"


typedef struct _listfile_t {
	int        fd;
	int        next_index;
	fwindow_t  head;
} listfile_t;

typedef uint32_t listfile_recsize_t;

errcode_t listfile_init(listfile_t * self, int fd);
errcode_t listfile_fini(listfile_t * self);
errcode_t listfile_append(listfile_t * self, const void * buffer,
		listfile_recsize_t size, OUT int * outindex);
errcode_t listfile_open(listfile_t * self, const char * filename);
errcode_t listfile_close(listfile_t * self);


#endif /* LISTFILE_H_INCLUDED */
