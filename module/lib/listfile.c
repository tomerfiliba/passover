#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "listfile.h"


errcode_t listfile_init(listfile_t * self, int fd)
{
	self->fd = fd;
	self->next_index = 0;
	return fwindow_init(&self->head, fd, 1024 * 1024);
}

errcode_t listfile_fini(listfile_t * self)
{
	return fwindow_fini(&self->head);
}

errcode_t listfile_append(listfile_t * self, const void * buffer,
		listfile_recsize_t size, int * outindex)
{
	if (outindex != NULL) {
		*outindex = self->next_index;
	}
	/*if (outoffset != NULL) {
		*outoffset = self->head.pos;
	}*/

	PROPAGATE(fwindow_write(&self->head, &size, sizeof(size)));
	PROPAGATE(fwindow_write(&self->head, buffer, size));
	self->next_index += 1;
	RETURN_SUCCESSFUL;
}

errcode_t listfile_open(listfile_t * self, const char * filename)
{
	int ret = 0;
	int fd = open(filename, O_RDWR | O_CREAT | O_TRUNC,
			S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
	if (fd < 0) {
		return ERR_LISTFILE_OPEN_FAILED;
	}
	ret = listfile_init(self, fd);
	if (IS_ERROR(ret)) {
		close(fd);
		self->fd = -1;
	}
	return ret;
}

errcode_t listfile_close(listfile_t * self)
{
	int ret = 0;
	ret = listfile_fini(self);
	if (self->fd >= 0) {
		close(self->fd);
		self->fd = -1;
	}
	return ret;
}






