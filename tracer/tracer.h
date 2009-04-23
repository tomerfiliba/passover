#ifndef TRACER_H_INCLUDED
#define TRACER_H_INCLUDED

#include "python.h"
#include "../lib/errors.h"
#include "../lib/htable.h"
#include "../lib/listfile.h"
#include "../lib/rotdir.h"
#include "../lib/rotrec.h"


typedef struct {
	int        depth;
	rotrec_t   records;
	listfile_t codepoints;
	htable_t   table;
} tracer_t;


errcode_t tracer_init(tracer_t * self, rotdir_t * dir, const char * prefix,
		const char * codepoints_filename);
errcode_t tracer_fini(tracer_t * self);
errcode_t tracer_log(tracer_t * self, PyObject * fmtstr, PyObject * args);
errcode_t tracer_pyfunc_call(tracer_t * self, PyCodeObject * code, int argcount, PyObject * args[]);
errcode_t tracer_pyfunc_return(tracer_t * self, PyObject * retval);
errcode_t tracer_cfunc_call(tracer_t * self, PyCFunctionObject * func);
errcode_t tracer_cfunc_return(tracer_t * self);
errcode_t tracer_raise(tracer_t * self, PyObject * excinfo);


#endif // TRACER_H_INCLUDED
