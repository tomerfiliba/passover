#ifndef TRACER_H_INCLUDED
#define TRACER_H_INCLUDED

#include "python.h"
#include "../lib/errors.h"
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


typedef struct {
	int        depth;
	rotrec_t   records;
	swriter_t  stream;
	listfile_t codepoints;
	htable_t   table;
} tracer_t;


errcode_t tracer_init(tracer_t * self, rotdir_t * dir, const char * prefix,
		const char * codepoints_filename, size_t map_size, size_t file_size);
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
