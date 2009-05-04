#ifndef TRACER_H_INCLUDED
#define TRACER_H_INCLUDED

#include "python.h"
#include "../lib/errors.h"
#include "../lib/hptime.h"
#include "../lib/htable.h"
#include "../lib/listfile.h"
#include "../lib/rotdir.h"
#include "../lib/rotrec.h"
#include "../lib/swriter.h"

#define TRACER_RECORD_INVALID 0
#define TRACER_RECORD_PYCALL  1
#define TRACER_RECORD_PYRET   2
#define TRACER_RECORD_PYRAISE 3
#define TRACER_RECORD_CCALL   4
#define TRACER_RECORD_CRET    5
#define TRACER_RECORD_CRAISE  6
#define TRACER_RECORD_LOG     7

#define TRACER_CODEPOINT_INVALID  0
#define TRACER_CODEPOINT_LOGLINE  1
#define TRACER_CODEPOINT_PYFUNC   2
#define TRACER_CODEPOINT_CFUNC    3

#define TRACER_PYOBJ_NONE       0
#define TRACER_PYOBJ_UNDUMPABLE 1
#define TRACER_PYOBJ_TRUE       2
#define TRACER_PYOBJ_FALSE      3
#define TRACER_PYOBJ_INT        4
#define TRACER_PYOBJ_LONG       5
#define TRACER_PYOBJ_FLOAT      6
#define TRACER_PYOBJ_STR        7
#define TRACER_PYOBJ_TYPE       8
#define TRACER_PYOBJ_OID        9

#define TRACER_PYOBJ_MIN_IMM_INT   (-20)
#define TRACER_PYOBJ_MAX_IMM_INT   (30)
#define TRACER_PYOBJ_IMMINT_0      50

#define TRACER_TIMEINDEX_INTERVAL  (1000000)

typedef struct {
	int        depth;
	usec_t     next_timestamp;
	rotrec_t   records;
	swriter_t  stream;
	swriter_t  cpstream;
	listfile_t codepoints;
	listfile_t timeindex;
	htable_t   table;
} tracer_t;


errcode_t tracer_init(tracer_t * self, rotdir_t * dir, const char * prefix,
		size_t map_size, size_t file_size);
errcode_t tracer_fini(tracer_t * self);
errcode_t tracer_log(tracer_t * self, PyObject * fmtstr, PyObject * argstuple);
errcode_t tracer_pyfunc_call(tracer_t * self, PyCodeObject * code, int argcount,
		PyObject * args[]);
errcode_t tracer_pyfunc_return(tracer_t * self, PyCodeObject * code,
		PyObject * retval);
errcode_t tracer_pyfunc_raise(tracer_t * self, PyCodeObject * code,
		PyObject * exctype);
errcode_t tracer_cfunc_call(tracer_t * self, PyCFunctionObject * func);
errcode_t tracer_cfunc_return(tracer_t * self, PyCFunctionObject * func);
errcode_t tracer_cfunc_raise(tracer_t * self, PyCFunctionObject * func,
		PyObject * exctype);


#endif // TRACER_H_INCLUDED
