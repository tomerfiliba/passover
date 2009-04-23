/*
 * Mapped files
 */

#ifndef FMAP_H_INCLUDED
#define FMAP_H_INCLUDED

#include <stdlib.h>
#include "errors.h"

#define FMAP_READ       (1)
#define FMAP_WRITE      (2)
#define FMAP_READ_AHEAD (4)
#define FMAP_LOCKED     (16)
#define FMAP_NOSWAP     (32)

typedef struct _fmap_t
{
	int    fd;
	size_t map_size;
	size_t physical_map_size;
	size_t map_ahead_size;
	off_t  map_offset;
	int    prot;
	int    flags;
	void * addr;
} fmap_t;

errcode_t fmap_init(fmap_t * self, int fd, int flags, size_t map_size, size_t map_ahead_size);
errcode_t fmap_fini(fmap_t * self);
errcode_t fmap_map(fmap_t * self, off_t offset, size_t size, OUT void ** outaddr);

typedef struct _fwindow_t
{
	fmap_t map;
	off_t  pos;
} fwindow_t;

errcode_t fwindow_init(fwindow_t * self, int fd, size_t map_size);
errcode_t fwindow_fini(fwindow_t * self);
errcode_t fwindow_write(fwindow_t * self, const void * buf, size_t size);
inline off_t fwindow_tell(fwindow_t * self);
inline void fwindow_advance(fwindow_t * self, off_t delta);


#endif /* FMAP_H_INCLUDED */
