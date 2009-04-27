#include "python.h"
#include <stdint.h>
#include <stdlib.h>

#include "../lib/errors.h"
#include "../lib/hptime.h"
#include "tracer.h"


errcode_t tracer_init(tracer_t * self, rotdir_t * dir, const char * prefix,
		const char * codepoints_filename, size_t map_size, size_t file_size)
{
	static const int MB = 1024 * 1024;
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
	PROPAGATE(swriter_dump_uint16(stream, num))

#define DUMP_UI32(num) \
	PROPAGATE(swriter_dump_uint32(stream, num))

#define DUMP_CSTR(str) \
	PROPAGATE(swriter_dump_cstr(stream, str))

#define DUMP_PYSTR(obj) \
	PROPAGATE(swriter_dump_pstr(stream, PyString_AS_STRING(obj), PyString_GET_SIZE(obj)))

#define DUMP_FINALIZE \
	return rotret_write(&self->records, swriter_get_buffer(&self->stream), \
				swriter_get_length(&self->stream))

errcode_t tracer_log(tracer_t * self, PyObject * fmtstr, PyObject * argstuple)
{
	DUMP_HEADER(TRACER_RECORD_LOG);
	DUMP_PYSTR(fmtstr);
	DUMP_FINALIZE;
}

/*errcode_t _tracer_codepoint()
{
#ifdef TRACER_DUMP_ABSPATH
	char * realpath = canonicalize_file_name(PyString_AS_STRING(codeobj->co_filename));
	if (realpath == NULL) {
		DUMP_PYSTR(sw, codeobj->co_filename);
	}
	else {
		DUMP_CSTR(sw, realpath);
		free(realpath);
	}
#else
	DUMP_PYSTR(sw, codeobj->co_filename);
#endif
}*/

static inline errcode_t _tracer_dump_argument(tracer_t * self, PyObject * obj)
{
	PyObject * repred = PyObject_Repr(retval);
	if (repred == NULL) {
		return ERR_TRACER_REPR_OF_ARGUMENT_FAILED;
	}
	DUMP_PYSTR(repred);
	Py_DECREF(repred);
	RETURN_SUCCESSFUL;
}

errcode_t tracer_pyfunc_call(tracer_t * self, PyCodeObject * code, int argcount, PyObject * args[])
{
	int i;
	PyObject * repred = NULL;

	DUMP_HEADER(TRACER_RECORD_PYCALL);
	DUMP_PYSTR(code->co_filename);
	DUMP_PYSTR(code->co_name);
	DUMP_UI32(code->co_firstlineno);
	DUMP_UI32(argcount);
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
	DUMP_PYSTR(code->co_filename);
	DUMP_PYSTR(code->co_name);
	DUMP_UI32(code->co_firstlineno);
	PROPAGATE(_tracer_dump_argument(self, retval);
	DUMP_FINALIZE;
}

errcode_t tracer_pyfunc_raise(tracer_t * self, PyCodeObject * code, PyObject * excinfo)
{
	DUMP_HEADER(TRACER_RECORD_PYRAISE);
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
	DUMP_FINALIZE;
}

errcode_t tracer_cfunc_raise(tracer_t * self, PyObject * excinfo)
{
	PyObject *
	self->depth -= 1;
	DUMP_HEADER(TRACER_RECORD_RAISE);
	PyTuple_GET_ITEM(excinfo, 0)
	DUMP_FINALIZE;
}

