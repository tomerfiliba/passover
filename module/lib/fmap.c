/*
 * FMap (file map) -- maps a portion of a file into memory, returning a
 * pointer to it. it is basically a wrapper around mmap(), that takes care of
 * rounding to page size, ensuring file capacity, etc. using fmap is much more
 * efficient than using write() and read() -- it does not require system calls
 * until the map is moved, and is crash consistent, because the kernel will
 * write the pages to disk when the process dies (no intermidiate buffering).
 * the fmap will not actually remap unless the requested address is out of
 * the range of the current map. this means that for most of the time, there
 * are no system calls.
 * Also, you can create more than a one fmap per file, mapping different parts
 * of it. This way you don't need to fseek() and flush the file buffers every
 * time.
 *
 * FWindow (file window) -- a sliding window over fmap. it advances the
 * window's position every time you write to it, like a stream.
 */
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include "fmap.h"

static long _fmap_page_size = 0;


/*
 * fd - an open file descriptor, with the right mode (reading/writing/etc)
 * flags - a combination of FMAP_{READ|WRITE|LOCK|NOSWAP|READ_AHEAD}
 * map_size - the total size of the mapped portion (usually ~5MB)
 * map_ahead_size - a hint used to determine where to start the maping,
 * relative to the user's desired offset:
 *
 *       (the offset the user requested)
 *               |
 * ..............X...........................................
 *     |-------------- total map size ---------------|
 *                |--------- map_ahead_size ---------|
 */
errcode_t fmap_init(fmap_t * self, int fd, int flags, size_t map_size, size_t map_ahead_size)
{
	if (_fmap_page_size == 0) {
		_fmap_page_size = sysconf(_SC_PAGE_SIZE);
	}
	if(map_ahead_size > map_size) {
		// map_ahead_size must not exceed map_size
		return ERR_FMAP_MAP_AHEAD_GREATER_THAN_MAP_SIZE;
	}

	self->fd = fd;
	self->map_size = map_size;
	self->physical_map_size = map_size + _fmap_page_size;
	if (self->physical_map_size % _fmap_page_size != 0) {
		// round up to page boundary
		self->physical_map_size =
			((self->physical_map_size / _fmap_page_size) + 1) * _fmap_page_size;
	}
	self->map_ahead_size = map_ahead_size;
	self->map_offset = 0;
	self->addr = NULL;
	self->prot = ((flags & FMAP_READ) ? PROT_READ : 0) |
	              ((flags & FMAP_WRITE) ? PROT_WRITE : 0);
	self->flags = MAP_SHARED |
	              ((flags & FMAP_READ_AHEAD) ? MAP_POPULATE : 0) |
	              ((flags & FMAP_LOCKED) ? MAP_LOCKED : 0) |
	              ((flags & FMAP_NOSWAP) ? MAP_NORESERVE : 0);
	RETURN_SUCCESSFUL;
}

//#define FMAP_BACKGROUND_MUNMAP

#ifdef FMAP_BACKGROUND_MUNMAP
#include <pthread.h>

typedef struct
{
	void * addr;
	size_t length;
} _munmap_args_t;

static void * _threaded_munmap(void * arg)
{
	_munmap_args_t munmap_args = *((_munmap_args_t*)arg);
	free(arg);
	munmap(munmap_args.addr, munmap_args.length);
	return NULL;
}

static inline void nonblocking_munmap(void * addr, size_t length)
{
	_munmap_args_t * munmap_args = (_munmap_args_t *)malloc(sizeof(_munmap_args_t));
	pthread_t t;

	if (munmap_args != NULL) {
		munmap_args->addr = addr;
		munmap_args->length = length;
		if (pthread_create(&t, NULL, _threaded_munmap, munmap_args) == 0) {
			return; // great success
		}
		// thread creation failed, release resources
		free(munmap_args);
	}

	// sorry, we have to block
	munmap(addr, length);
}
#endif // FMAP_BACKGROUND_MUNMAP

static inline void _fmap_unmap(fmap_t * self)
{
	#ifdef FMAP_BACKGROUND_MUNMAP
	nonblocking_munmap(self->addr, self->physical_map_size);
	#else
	munmap(self->addr, self->physical_map_size);
	#endif
	self->addr = NULL;
}

errcode_t fmap_fini(fmap_t * self)
{
	self->fd = -1;
	if (self->addr != NULL) {
		_fmap_unmap(self);
	}
	RETURN_SUCCESSFUL;
}

static inline errcode_t _fmap_ensure_file_capacity(int fd, off_t length)
{
	struct stat sb;
	if (fstat(fd, &sb) != 0) {
		return ERR_FMAP_STAT_FAILED;
	}
	if (sb.st_size >= length) {
		RETURN_SUCCESSFUL; // no op
	}
	if (ftruncate(fd, length) != 0) {
		return ERR_FMAP_TRUNCATE_FAILED;
	}
	RETURN_SUCCESSFUL;
}

errcode_t fmap_map(fmap_t * self, off_t offset, size_t size, OUT void ** outaddr)
{
	off_t abs_offset, page_offset, end_offset, back_size, fwd_size;
	void * addr;

	// mapping with size = 0 only ensures the memory region

	// cannot map more than map_size
	if (size > self->map_size) {
		return ERR_FMAP_MAP_TOO_BIG;
	}

	if (self->addr != NULL && (offset < self->map_offset ||
			offset + size > self->map_offset + self->physical_map_size)) {
		// the requested offset and size are out of the range of the
		// current map
		_fmap_unmap(self);
	}

	if (self->addr == NULL) {
		fwd_size = size + self->map_ahead_size;
		if (fwd_size > self->map_size) {
			fwd_size = self->map_size; // map_ahead_size is only a hint
		}
		back_size = self->map_size - fwd_size;
		abs_offset = (offset > back_size) ? offset - back_size : 0;
		page_offset = (abs_offset / _fmap_page_size) * _fmap_page_size;
		end_offset = page_offset + self->physical_map_size;
		if (offset + size > end_offset) {
			// alignment problems -- shouldn't happen since physical_map_size
			// is rounded up
			return ERR_FMAP_ALIGNMENT_ERROR;
		}
		PROPAGATE(_fmap_ensure_file_capacity(self->fd, end_offset));
		addr = mmap(NULL, self->physical_map_size, self->prot, self->flags,
				self->fd, page_offset);
		if (addr == MAP_FAILED) {
			return ERR_FMAP_MMAP_FAILED; // check errno
		}
		self->map_offset = page_offset;
		self->addr = addr;
	}

	*outaddr = self->addr + (offset - self->map_offset);
	RETURN_SUCCESSFUL;
}

/*
 * sliding window over fmap
 */
errcode_t fwindow_init(fwindow_t * self, int fd, size_t map_size)
{
	self->pos = 0;
	return fmap_init(&self->map, fd, FMAP_WRITE, map_size, map_size);
}

errcode_t fwindow_fini(fwindow_t * self)
{
	return fmap_fini(&self->map);
}

errcode_t fwindow_write(fwindow_t * self, const void * buf, size_t size)
{
	void *addr = NULL;
	PROPAGATE(fmap_map(&self->map, self->pos, size, &addr));
	if (buf != NULL) {
		memcpy(addr, buf, size);
	}
	self->pos += size;
	return 0;
}

inline off_t fwindow_tell(fwindow_t * self)
{
	return self->pos;
}

inline void fwindow_advance(fwindow_t * self, off_t delta)
{
	self->pos += delta;
}

/*
int main()
{
	fwindow_t w;
	char buf[500];
	int i;

	int fd = open("/tmp/foo", O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR);
	if (fd < 0) {
		printf("open failed\n");
		return 1;
	}
	ASSERT(fwindow_init(&w, fd, 4000));
	for (i = 0; i < 100; i++) {
		printf("%d\n", i);
		ASSERT(fwindow_write(&w, buf, sizeof(buf)));
	}

	ASSERT(fwindow_fini(&w));
	close(fd);
	return 0;
}
*/

