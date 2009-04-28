/*
 * Rotated records
 */

#ifndef ROTREC_H_INCLUDED
#define ROTREC_H_INCLUDED

#include <stdlib.h>
#include <stdint.h>
#include "fmap.h"
#include "rotdir.h"


typedef struct {
	int        flags;
	rotdir_t * rotdir;
	int        rotdir_slot;
	fwindow_t  window;
	off_t      file_size;
	size_t     map_size;
	char       file_prefix[ROTDIR_MAX_FILEPREFIX_LEN];
} rotrec_t;


typedef uint16_t rotret_record_size_t;

int rotrec_init(rotrec_t * self, rotdir_t * rotdir, const char * file_prefix,
		size_t map_size, off_t file_size);
int rotrec_fini(rotrec_t * self);
int rotrec_write(rotrec_t * self, const void * buf, rotret_record_size_t size);


#endif /* ROTREC_H_INCLUDED */
