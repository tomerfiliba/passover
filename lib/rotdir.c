#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "rotdir.h"


errcode_t rotdir_init(rotdir_t * self, const char * path, int max_files)
{
	errcode_t retcode = ERR_UNKNOWN;

	if (strlen(path) > sizeof(self->path) - (ROTDIR_MAX_FILENAME_LEN + 2)) {
		retcode = ERR_ROTDIR_PATH_TOO_LONG;
		goto error1;
	}
	strncpy(self->path, path, sizeof(self->path));
	self->max_files = max_files;
	self->alloc_counter = 0;
	self->dealloc_counter = 0;

	self->files = malloc(sizeof(_rotdir_fileinfo_t) * max_files);
	if (self->files == NULL) {
		retcode = ERR_ROTDIR_MALLOC_FAILED;
		goto error1;
	}
	memset(self->files, 0, sizeof(_rotdir_fileinfo_t) * max_files);

	if (pthread_mutex_init(&self->mutex, NULL) != 0) {
		retcode = ERR_ROTDIR_MUTEX_INIT_FAILED;
		goto error2;
	}

	RETURN_SUCCESSFUL;

error2:
	free(self->files);
error1:
	self->files = NULL;
	self->path[0] = '\0';
	return retcode;
}

errcode_t rotdir_fini(rotdir_t * self)
{
	if (self->files != NULL) {
		free(self->files);
		self->files = NULL;
		self->path[0] = '\0';
		pthread_mutex_destroy(&self->mutex);
	}

	RETURN_SUCCESSFUL;
}

static inline errcode_t _rotdir_get_free_slot(rotdir_t * self, OUT int * slot)
{
	int i;
	int oldest_dealloc_order = -1;
	int oldest_index = -1;
	char filename[PATH_MAX];

	for(i = 0; i < self->max_files; i++) {
		if (!self->files[i].allocated) {
			if (self->files[i].filename[0] == '\0') {
				*slot = i; // we have an empty slot, no need to rotate
				RETURN_SUCCESSFUL;
			}

			if (oldest_dealloc_order < 0 || oldest_dealloc_order > self->files[i].dealloc_order) {
				oldest_dealloc_order  = self->files[i].dealloc_order;
				oldest_index = i;
			}
		}
	}

	if (oldest_index < 0) {
		return ERR_ROTDIR_OUT_OF_SLOTS; // all slots are allocated
	}
	snprintf(filename, sizeof(filename), "%s/%s", self->path,
			self->files[oldest_index].filename);
	if (unlink(filename) != 0) {
		return ERR_ROTDIR_UNLINK_FAILED;
	}
	self->files[oldest_index].filename[0] = '\0';
	*slot = oldest_index;
	RETURN_SUCCESSFUL;
}

errcode_t rotdir_allocate(rotdir_t * self, const char * prefix, OUT int * outslot,
		OUT char * outfilename)
{
	int slot;
	errcode_t retcode = ERR_UNKNOWN;

	if (strlen(prefix) > ROTDIR_MAX_FILEPREFIX_LEN) {
		return ERR_ROTDIR_PREFIX_TOO_LONG;
	}

	pthread_mutex_lock(&self->mutex);
	retcode = _rotdir_get_free_slot(self, &slot);
	if (IS_ERROR(retcode)) {
		goto cleanup;
	}

	self->files[slot].allocated = 1;
	snprintf(self->files[slot].filename, sizeof(self->files[slot].filename),
		"%s.%06d.rot", prefix, self->alloc_counter);
	self->alloc_counter += 1;
	snprintf(outfilename, PATH_MAX, "%s/%s", self->path, self->files[slot].filename);

	*outslot = slot;
	retcode = ERR_SUCCESS;

cleanup:
	pthread_mutex_unlock(&self->mutex);
	return retcode;
}

errcode_t rotdir_deallocate(rotdir_t * self, int slot)
{
	if (slot < 0 || slot >= self->max_files) {
		return ERR_ROTDIR_INVALID_SLOT;
	}

	pthread_mutex_lock(&self->mutex);
	self->files[slot].allocated = 0;
	self->files[slot].dealloc_order = self->dealloc_counter;
	self->dealloc_counter += 1;
	pthread_mutex_unlock(&self->mutex);
	RETURN_SUCCESSFUL;
}

/*
#include <fcntl.h>

int main()
{
	int i;
	int slot;
	char fn[PATH_MAX];
	rotdir_t rd;

	system("rm -rf /tmp/lalala");
	system("mkdir /tmp/lalala");
	ASSERT(rotdir_init(&rd, "/tmp/lalala", 7));

	for (i = 0; i < 100; i++) {
		ASSERT(rotdir_allocate(&rd, "moshe", &slot, fn));
		printf("file: %d, %s\n", slot, fn);
		close(open(fn, O_RDWR | O_CREAT, 0700));
		rotdir_deallocate(&rd, slot);
		printf("\n");
	}

	ASSERT(rotdir_fini(&rd));
	return 0;
}
*/

