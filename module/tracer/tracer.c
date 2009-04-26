#include "python.h"
#include <stdint.h>

#include "../lib/hptime.h"
#include "tracer.h"


#define TRACER_RECORD_PY_ENTER
#define TRACER_RECORD_PY_LEAVE
#define TRACER_RECORD_PY_EXC
#define TRACER_RECORD_C_ENTER
#define TRACER_RECORD_C_LEAVE
#define TRACER_RECORD_C_EXC
#define TRACER_RECORD_LOG


#pragma pack(1)

typedef struct {
	char     type;                   // TRACER_RECORD_LOG
	uint16_t cp_index;               // code point index
	uint64_t timestamp;
	// payload follows
} _tracer_log_rec_t;

typedef struct {
	char     type;                   // TRACER_RECORD_PY_XXX
	uint16_t cp_index;               // code point index
	// payload follows
} _tracer_pyfunc_rec_t;

#pragma pack()


errcode_t tracer_init(tracer_t * self, rotdir_t * dir, const char * prefix, const char * codepoints_filename)
{
	static const int MB = 1024 * 1024;
	errcode_t retcode = ERR_UNKNOWN;

	PROPAGATE_TO(error1, retcode = htable_init(&self->table, 65535));
	PROPAGATE_TO(error2, retcode = listfile_open(&self->codepoints, codepoints_filename));
	PROPAGATE_TO(error3, retcode = rotrec_init(&self->records, dir, prefix, 2 * MB, 100 * MB));

	RETURN_SUCCESSFUL;

error3:
	listfile_close(&self->codepoints);
error2:
	htable_fini(&self->table);
error1:
	return retcode;
}

errcode_t tracer_fini(tracer_t * self)
{
	PROPAGATE(rotrec_fini(&self->records));
	PROPAGATE(listfile_fini(&self->codepoints));
	PROPAGATE(htable_fini(&self->table));
	RETURN_SUCCESSFUL;
}

typedef struct {
	usec_t timestamp;
} logline_t;

errcode_t tracer_log(tracer_t * self, PyObject * fmtstr, PyObject * argstuple)
{
	logline_t info;
	info.timestamp = hptime_get_time();
	//printf("LOG\n");
	RETURN_SUCCESSFUL;
}

errcode_t tracer_pyfunc_call(tracer_t * self, PyCodeObject * code, int argcount, PyObject * args[])
{
	printf("PYCALL\n");
	RETURN_SUCCESSFUL;
}

errcode_t tracer_pyfunc_return(tracer_t * self, PyObject * retval)
{
	printf("PYRET\n");
	RETURN_SUCCESSFUL;
}

errcode_t tracer_cfunc_call(tracer_t * self, PyCFunctionObject * func)
{
	//printf("CCALL\n");
	RETURN_SUCCESSFUL;
}

errcode_t tracer_cfunc_return(tracer_t * self)
{
	//printf("CRET\n");
	RETURN_SUCCESSFUL;
}

errcode_t tracer_raise(tracer_t * self, PyObject * excinfo)
{
	//printf("EXC\n");
	RETURN_SUCCESSFUL;
}

