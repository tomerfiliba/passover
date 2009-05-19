#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "rotrec.h"


#define ROTREC_FLAG_WINDOW_OPENED          0x0001
#define ROTREC_FLAG_INCREMENT_BASE_OFFSET  0x0002
#define ROTREC_FILE_HEADER_SIZE            (sizeof(uint64_t))


errcode_t rotrec_init(rotrec_t * self, rotdir_t * rotdir, const char * file_prefix,
		size_t map_size, off_t file_data_size)
{
	if (map_size > file_data_size) {
		return ERR_ROTREC_MAPSIZE_GREATER_THAN_FILESIZE;
	}
	if (strlen(file_prefix) > sizeof(self->file_prefix)) {
		return ERR_ROTREC_FILEPREFIX_TOO_LONG;
	}

	self->flags = 0;
	self->base_offset = 0;
	self->rotdir_slot = -1;
	self->rotdir = rotdir;
	self->file_data_size = file_data_size;
	self->total_file_size = file_data_size + ROTREC_FILE_HEADER_SIZE;
	self->map_size = map_size;
	strncpy(self->file_prefix, file_prefix, sizeof(self->file_prefix));

	RETURN_SUCCESSFUL;
}

static inline errcode_t _rotrec_close_window(rotrec_t * self)
{
	if (self->rotdir_slot >= 0) {
		PROPAGATE(rotdir_deallocate(self->rotdir, self->rotdir_slot));
		self->rotdir_slot = -1;
	}

	PROPAGATE(fwindow_fini(&self->window));
	self->flags &= ~ROTREC_FLAG_WINDOW_OPENED;
	self->flags |= ROTREC_FLAG_INCREMENT_BASE_OFFSET;

	RETURN_SUCCESSFUL;
}

static inline errcode_t _rotrec_open_window(rotrec_t * self)
{
	errcode_t retcode = ERR_UNKNOWN;
	char filename[PATH_MAX];
	int fd;
	int slot;

	PROPAGATE_TO(error1, retcode = rotdir_allocate(self->rotdir, self->file_prefix,
			&slot, filename));
	fd = open(filename, O_RDWR | O_CREAT | O_EXCL,
			S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
	if (fd < 0) {
		retcode = ERR_ROTREC_OPEN_FAILED;
		goto error2;
	}
	PROPAGATE_TO(error3, retcode = fwindow_init(&self->window, fd, self->map_size));

	uint64_t base_offset = (uint64_t)(self->base_offset);

	if (self->flags & ROTREC_FLAG_INCREMENT_BASE_OFFSET) {
		self->flags &= ~ROTREC_FLAG_INCREMENT_BASE_OFFSET;
		base_offset += self->total_file_size;
	}

	PROPAGATE_TO(error4, retcode = fwindow_write(&self->window, &base_offset,
			sizeof(base_offset)));

	self->flags |= ROTREC_FLAG_WINDOW_OPENED;
	self->rotdir_slot = slot;
	self->base_offset = base_offset;

	RETURN_SUCCESSFUL;

error4:
	fwindow_fini(&self->window);
error3:
	close(fd);
error2:
	rotdir_deallocate(self->rotdir, slot);
error1:
	return retcode;
}

static inline int _rotrec_ensure(rotrec_t * self, size_t size)
{
	if (self->flags & ROTREC_FLAG_WINDOW_OPENED) {
		if (fwindow_tell(&self->window) + size > self->total_file_size) {
			// record will not fit in this file, so we need to close it
			PROPAGATE(_rotrec_close_window(self));
		}
	}
	if (!(self->flags & ROTREC_FLAG_WINDOW_OPENED)) {
		PROPAGATE(_rotrec_open_window(self));
	}

	RETURN_SUCCESSFUL;
}

errcode_t rotrec_fini(rotrec_t * self)
{
	if (self->flags & ROTREC_FLAG_WINDOW_OPENED) {
		PROPAGATE(_rotrec_close_window(self));
	}

	self->flags = 0;
	self->rotdir = NULL;
	self->rotdir_slot = -1;
	RETURN_SUCCESSFUL;
}

errcode_t rotrec_write(rotrec_t * self, const void * buf, rotret_record_size_t size, off_t * outoffset)
{
	if (size > self->file_data_size) {
		return ERR_ROTREC_SIZE_TOO_LARGE;
	}

	PROPAGATE(_rotrec_ensure(self, size));
	if (outoffset != NULL) {
		*outoffset = self->base_offset + fwindow_tell(&self->window);
	}
	PROPAGATE(fwindow_write(&self->window, &size, sizeof(size)));
	PROPAGATE(fwindow_write(&self->window, buf, size));

	RETURN_SUCCESSFUL;
}


/*
int main()
{
	rotrec_t rc;
	rotdir_t rd;

	system("rm -rf /tmp/lalala");
	system("mkdir /tmp/lalala");
	ASSERT(rotdir_init(&rd, "/tmp/lalala", 5));
	ASSERT(rotrec_init(&rc, &rd, "thread-1", 4 * 1024, 40 * 1024));

	char data[500] = {0};
	int i;
	for (i = 0; i < 1000; i++) {
		sprintf(data, "%d ", i);
		ASSERT(rotrec_write(&rc, data, sizeof(data)));
	}

	ASSERT(rotrec_fini(&rc));
	ASSERT(rotdir_fini(&rd));

	return 0;
}
*/




