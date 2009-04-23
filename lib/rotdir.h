/*
 * Rotated directories
 */

#ifndef ROTDIR_H_INCLUDED
#define ROTDIR_H_INCLUDED

#include <limits.h>
#include <pthread.h>

#include "errors.h"

#define ROTDIR_MAX_FILENAME_LEN    (100)
#define ROTDIR_MAX_FILEPREFIX_LEN  (ROTDIR_MAX_FILENAME_LEN - 20)


typedef struct {
	int      allocated;
	int      dealloc_order;
	char     filename[ROTDIR_MAX_FILENAME_LEN];
} _rotdir_fileinfo_t;

typedef struct {
	char                 path[PATH_MAX];
	int                  max_files;
	int                  alloc_counter;
	int                  dealloc_counter;
	_rotdir_fileinfo_t * files;
	pthread_mutex_t      mutex;
} rotdir_t;

errcode_t rotdir_init(rotdir_t * self, const char * path, int max_files);
errcode_t rotdir_fini(rotdir_t * self);
errcode_t rotdir_allocate(rotdir_t * self, const char * prefix, OUT int * outslot, OUT char * outfilename);
errcode_t rotdir_deallocate(rotdir_t * self, int slot);


#endif /* ROTDIR_H_INCLUDED */
