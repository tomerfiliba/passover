#include "python.h"
#include <stdint.h>
#include <stdlib.h>

#include "../lib/errors.h"
#include "../lib/hptime.h"
#include "tracer.h"


errcode_t tracer_init(tracer_t * self, rotdir_t * dir, const char * prefix,
		const char * codepoints_filename, size_t map_size, size_t file_size)
{
	errcode_t retcode = ERR_UNKNOWN;

	self->depth = 0;

	PROPAGATE_TO(error1, retcode = htable_init(&self->table, 65535));
	PROPAGATE_TO(error2, retcode = swriter_init(&self->stream, 16*1024));
	PROPAGATE_TO(error3, retcode = listfile_open(&self->codepoints, codepoints_filename));
	PROPAGATE_TO(error4, retcode = rotrec_init(&self->records, dir, prefix, map_size, file_size));

	RETURN_SUCCESSFUL;

error4:
	listfile_close(&self->codepoints);
error3:
	swriter_fini(&self->stream);
error2:
	htable_fini(&self->table);
error1:
	return retcode;
}

errcode_t tracer_fini(tracer_t * self)
{
	PROPAGATE(rotrec_fini(&self->records));
	PROPAGATE(listfile_fini(&self->codepoints));
	PROPAGATE(swriter_fini(&self->stream));
	PROPAGATE(htable_fini(&self->table));
	RETURN_SUCCESSFUL;
}

#define DUMP_HEADER(type) \
	PROPAGATE(swriter_clear(&self->stream)); \
    PROPAGATE(swriter_dump_uint8(&self->stream, type)); \
    PROPAGATE(swriter_dump_uint16(&self->stream, self->depth)); \
	PROPAGATE(swriter_dump_uint64(&self->stream, hptime_get_time()))

#define DUMP_UI16(num) \
	PROPAGATE(swriter_dump_uint16(&self->stream, num))

#define DUMP_UI32(num) \
	PROPAGATE(swriter_dump_uint32(&self->stream, num))

#define DUMP_CSTR(str) \
	PROPAGATE(swriter_dump_cstr(&self->stream, str))

#define DUMP_PYSTR(obj) \
	PROPAGATE(swriter_dump_pstr(&self->stream, PyString_AS_STRING(obj), PyString_GET_SIZE(obj)))

#define DUMP_FINALIZE \
	return rotrec_write(&self->records, swriter_get_buffer(&self->stream), \
				swriter_get_length(&self->stream))

errcode_t tracer_log(tracer_t * self, PyObject * fmtstr, PyObject * argstuple)
{
	DUMP_HEADER(TRACER_RECORD_LOG);
	DUMP_PYSTR(fmtstr);
	DUMP_FINALIZE;
}

static inline errcode_t _tracer_dump_argument(tracer_t * self, PyObject * obj)
{
	PyObject * repred = PyObject_Repr(obj);
	if (repred == NULL) {
		return ERR_TRACER_REPR_OF_ARGUMENT_FAILED;
	}
	DUMP_PYSTR(repred); // ref leak!!
	Py_DECREF(repred);
	RETURN_SUCCESSFUL;
}

static inline errcode_t _tracer_dump_pyfunc(tracer_t * self, PyCodeObject * code)
{
#ifdef TRACER_DUMP_ABSPATH
	char * realpath = canonicalize_file_name(PyString_AS_STRING(code->co_filename));
	if (realpath == NULL) {
		DUMP_PYSTR(code->co_filename);
	}
	else {
		DUMP_CSTR(realpath);
		free(realpath);
	}
#else
	DUMP_PYSTR(code->co_filename);
#endif
	DUMP_PYSTR(code->co_name);
	DUMP_UI32(code->co_firstlineno);
	RETURN_SUCCESSFUL;
}

static inline errcode_t _tracer_dump_exception(tracer_t * self, PyObject * exctype)
{
	return _tracer_dump_argument(self, exctype);
}

errcode_t tracer_pyfunc_call(tracer_t * self, PyCodeObject * code, int argcount, PyObject * args[])
{
	int i;

	DUMP_HEADER(TRACER_RECORD_PYCALL);
	PROPAGATE(_tracer_dump_pyfunc(self, code));
	DUMP_UI16(argcount);
	for (i = 0; i < argcount; i++) {
		PROPAGATE(_tracer_dump_argument(self, args[i]));
	}

	self->depth += 1;
	DUMP_FINALIZE;
}

errcode_t tracer_pyfunc_return(tracer_t * self, PyCodeObject * code, PyObject * retval)
{
	self->depth -= 1;
	DUMP_HEADER(TRACER_RECORD_PYRET);
	PROPAGATE(_tracer_dump_pyfunc(self, code));
	PROPAGATE(_tracer_dump_argument(self, retval));
	DUMP_FINALIZE;
}

errcode_t tracer_pyfunc_raise(tracer_t * self, PyCodeObject * code,
		PyObject * exctype)
{
	if (exctype == NULL) {
		return ERR_TRACER_NO_EXCEPTION_SET;
	}

	DUMP_HEADER(TRACER_RECORD_PYRAISE);
	PROPAGATE(_tracer_dump_pyfunc(self, code));
	PROPAGATE(_tracer_dump_exception(self, exctype));
	DUMP_FINALIZE;
}

errcode_t _tracer_dump_cfunc(tracer_t * self, PyCFunctionObject * func)
{
	if (func->m_module == NULL) {
		DUMP_CSTR("");
	}
	else {
		PyObject * obj = PyObject_Str(func->m_module);
		if (obj == NULL) {
			return -1;
		}
		errcode_t code = swriter_dump_pstr(&self->stream, PyString_AS_STRING(obj),
				PyString_GET_SIZE(obj));
		Py_DECREF(obj);
		PROPAGATE(code);
	}
	DUMP_CSTR(func->m_ml->ml_name);

	RETURN_SUCCESSFUL;
}

errcode_t tracer_cfunc_call(tracer_t * self, PyCFunctionObject * func)
{
	DUMP_HEADER(TRACER_RECORD_CCALL);
	PROPAGATE(_tracer_dump_cfunc(self, func));

	self->depth += 1;
	DUMP_FINALIZE;
}

errcode_t tracer_cfunc_return(tracer_t * self, PyCFunctionObject * func)
{
	self->depth -= 1;
	DUMP_HEADER(TRACER_RECORD_CRET);
	PROPAGATE(_tracer_dump_cfunc(self, func));
	DUMP_FINALIZE;
}

errcode_t tracer_cfunc_raise(tracer_t * self, PyCFunctionObject * func,
		PyObject * exctype)
{
	if (exctype == NULL) {
		return ERR_TRACER_NO_EXCEPTION_SET;
	}
	self->depth -= 1;
	DUMP_HEADER(TRACER_RECORD_CRAISE);
	PROPAGATE(_tracer_dump_cfunc(self, func));
	PROPAGATE(_tracer_dump_exception(self, exctype));
	DUMP_FINALIZE;
}

