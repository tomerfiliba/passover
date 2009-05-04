#include "python.h"
#include <stdint.h>
#include <stdlib.h>

#include "tracer.h"


typedef htable_value_t codepoint_t;


errcode_t tracer_init(tracer_t * self, rotdir_t * dir, const char * prefix,
		size_t map_size, size_t file_size)
{
	char tmpfilename[PATH_MAX];
	errcode_t retcode = ERR_UNKNOWN;

	self->depth = 0;
	self->next_timestamp = 0;

	PROPAGATE_TO(error1, retcode = htable_init(&self->table, 65535));
	PROPAGATE_TO(error2, retcode = swriter_init(&self->stream, NULL, 16*1024));
	PROPAGATE_TO(error3, retcode = swriter_init(&self->cpstream, NULL, 16*1024));

	sprintf(tmpfilename, "%s/%s.codepoints", dir->path, prefix);
	PROPAGATE_TO(error4, retcode = listfile_open(&self->codepoints, tmpfilename));

	sprintf(tmpfilename, "%s/%s.timeindex", dir->path, prefix);
	PROPAGATE_TO(error5, retcode = listfile_open(&self->timeindex, tmpfilename));

	PROPAGATE_TO(error6, retcode = rotrec_init(&self->records, dir, prefix, map_size, file_size));

	RETURN_SUCCESSFUL;

error6:
	listfile_close(&self->timeindex);
error5:
	listfile_close(&self->codepoints);
error4:
	swriter_fini(&self->cpstream);
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
	PROPAGATE(listfile_fini(&self->timeindex));
	PROPAGATE(listfile_fini(&self->codepoints));
	PROPAGATE(swriter_fini(&self->cpstream));
	PROPAGATE(swriter_fini(&self->stream));
	PROPAGATE(htable_fini(&self->table));
	RETURN_SUCCESSFUL;
}

/****************************************************************************
 * Serializing values
 ***************************************************************************/

#define DUMP_UI8(stream, num) \
	PROPAGATE(swriter_dump_uint8(stream, num))

#define DUMP_UI16(stream, num) \
	PROPAGATE(swriter_dump_uint16(stream, num))

#define DUMP_UI32(stream, num) \
	PROPAGATE(swriter_dump_uint32(stream, num))

#define DUMP_UI64(stream, num) \
	PROPAGATE(swriter_dump_uint64(stream, num))

#define DUMP_CSTR(stream, str) \
	PROPAGATE(swriter_dump_cstr(stream, str))

#define DUMP_PYSTR(stream, obj) \
	PROPAGATE(swriter_dump_pstr(stream, PyString_AS_STRING(obj), PyString_GET_SIZE(obj)))

static inline int _tracer_hash_pyobject(void * obj)
{
	return ((int)((uintptr_t)obj)) >> 3;
}

static inline errcode_t _tracer_save_codeobj(swriter_t * cpstream, PyCodeObject * code)
{
	DUMP_UI8(cpstream, TRACER_CODEPOINT_PYFUNC);
#ifdef TRACER_DUMP_ABSPATH
	char * realpath = canonicalize_file_name(PyString_AS_STRING(code->co_filename));
	if (realpath == NULL) {
		DUMP_PYSTR(cpstream, code->co_filename);
	}
	else {
		DUMP_CSTR(cpstream, realpath);
		free(realpath);
	}
#else
	DUMP_PYSTR(cpstream, code->co_filename);
#endif

	DUMP_PYSTR(cpstream, code->co_name);
	DUMP_UI32(cpstream, code->co_firstlineno);

	RETURN_SUCCESSFUL;
}

static inline errcode_t _tracer_save_cfunc(swriter_t * cpstream, PyCFunctionObject * func)
{
	DUMP_UI8(cpstream, TRACER_CODEPOINT_CFUNC);
	if (func->m_module == NULL) {
		DUMP_CSTR(cpstream, "");
	}
	else {
		PyObject * obj = PyObject_Str(func->m_module);
		if (obj == NULL) {
			return -1;
		}
		errcode_t code = swriter_dump_pstr(cpstream, PyString_AS_STRING(obj),
				PyString_GET_SIZE(obj));
		Py_DECREF(obj);
		PROPAGATE(code);
	}
	DUMP_CSTR(cpstream, func->m_ml->ml_name);

	RETURN_SUCCESSFUL;
}

static inline errcode_t _tracer_save_logline(swriter_t * cpstream, PyObject * obj)
{
	if (!PyString_CheckExact(obj)) {
		return ERR_TRACER_LOGLINE_NOT_STRING;
	}
	DUMP_UI8(cpstream, TRACER_CODEPOINT_LOGLINE);
	DUMP_PYSTR(cpstream, obj);
	RETURN_SUCCESSFUL;
}

#define TRACER_GET_CODEPOINT(SAVER) \
	errcode_t retcode = ERR_UNKNOWN; \
	int hash = _tracer_hash_pyobject(obj); \
	\
	retcode = htable_get(&self->table, hash, (htable_key_t)((uintptr_t)obj), outvalue); \
	if (retcode == ERR_SUCCESS) { \
		RETURN_SUCCESSFUL; \
	} \
	else if (retcode == ERR_HTABLE_GET_KEY_MISSING) { \
		int index; \
		PROPAGATE(swriter_clear(&self->cpstream)); \
		PROPAGATE(SAVER(&self->cpstream, obj)); \
		PROPAGATE(listfile_append(&self->codepoints, swriter_get_buffer(&self->cpstream), \
				swriter_get_length(&self->cpstream), &index)); \
		PROPAGATE(htable_set(&self->table, hash, (htable_key_t)((uintptr_t)obj), (htable_value_t)index)); \
		*outvalue = (codepoint_t)index; \
		RETURN_SUCCESSFUL; \
	} \
	else { \
		return retcode; \
	} \

static inline errcode_t _tracer_get_codeobj_codepoint(tracer_t * self,
		PyCodeObject * obj, codepoint_t * outvalue)
{
	TRACER_GET_CODEPOINT(_tracer_save_codeobj)
}

static inline errcode_t _tracer_get_cfunc_codepoint(tracer_t * self,
		PyCFunctionObject * obj, codepoint_t * outvalue)
{
	TRACER_GET_CODEPOINT(_tracer_save_cfunc)
}
static inline errcode_t _tracer_get_logline_codepoint(tracer_t * self,
		PyObject * obj, codepoint_t * outvalue)
{
	TRACER_GET_CODEPOINT(_tracer_save_logline)
}

#define TRACER_DUMP_OBJ(CONVERTOR, ERRCODE) \
	errcode_t retcode = ERR_UNKNOWN; \
	PyObject * strobj = CONVERTOR(obj); \
	size_t size; \
	if (strobj == NULL) { \
		return ERRCODE; \
	} \
	size = PyString_GET_SIZE(strobj); \
	if (max_size > 0 && size > max_size) { \
		size = max_size; \
	} \
	retcode = swriter_dump_pstr(stream, PyString_AS_STRING(strobj), size); \
	Py_DECREF(strobj); \
	return retcode; \

static inline errcode_t _tracer_dump_obj_repr(swriter_t * stream, PyObject * obj, int max_size)
{
	TRACER_DUMP_OBJ(PyObject_Repr, ERR_TRACER_STRINGIFY_PYOBJECT_FAILED)
}

static inline errcode_t _tracer_dump_obj_str(swriter_t * stream, PyObject * obj, int max_size)
{
	TRACER_DUMP_OBJ(PyObject_Str, ERR_TRACER_STRINGIFY_PYOBJECT_FAILED)
}

static inline errcode_t _tracer_dump_argument(tracer_t * self, PyObject * obj)
{
	if (obj == Py_None) {
		DUMP_UI8(&self->stream, TRACER_PYOBJ_NONE);
	}
	else if (PyBool_Check(obj)) {
		if (obj == Py_True) {
			DUMP_UI8(&self->stream, TRACER_PYOBJ_TRUE);
		}
		else {
			DUMP_UI8(&self->stream, TRACER_PYOBJ_FALSE);
		}
	}
	else if (PyInt_CheckExact(obj)) {
		long val = ((PyIntObject*)obj)->ob_ival;
		if (val >= TRACER_PYOBJ_MIN_IMM_INT && val <= TRACER_PYOBJ_MAX_IMM_INT) {
			DUMP_UI8(&self->stream, TRACER_PYOBJ_IMMINT_0 + val);
		}
		else {
			DUMP_UI8(&self->stream, TRACER_PYOBJ_INT);
			PROPAGATE(_tracer_dump_obj_repr(&self->stream, obj, -1));
		}
	}
	else if (PyLong_CheckExact(obj)) {
		DUMP_UI8(&self->stream, TRACER_PYOBJ_LONG);
		PROPAGATE(_tracer_dump_obj_repr(&self->stream, obj, -1));
	}
	else if (PyFloat_CheckExact(obj)) {
		DUMP_UI8(&self->stream, TRACER_PYOBJ_FLOAT);
		PROPAGATE(_tracer_dump_obj_str(&self->stream, obj, 50));
	}
	else if (PyString_CheckExact(obj)) {
		DUMP_UI8(&self->stream, TRACER_PYOBJ_STR);
		PROPAGATE(_tracer_dump_obj_str(&self->stream, obj, 50));
	}
	else {
		DUMP_UI8(&self->stream, TRACER_PYOBJ_UNDUMPABLE);
		//PROPAGATE(_tracer_dump_obj_repr(&self->stream, (PyObject*)obj->ob_type, 50));
	}

	RETURN_SUCCESSFUL;
}

static inline errcode_t _tracer_dump_exception(tracer_t * self, PyObject * exctype)
{
	return _tracer_dump_obj_repr(&self->stream, exctype, -1);
}

/****************************************************************************
 * Trace records
 ***************************************************************************/

#define RECORD_HEADER(TYPE, GET_CP_FUNC, OBJ) \
	usec_t _timestamp = hptime_get_time(); \
	PROPAGATE(swriter_clear(&self->stream)); \
    PROPAGATE(swriter_dump_uint8(&self->stream, TYPE)); \
    PROPAGATE(swriter_dump_uint16(&self->stream, self->depth)); \
	PROPAGATE(swriter_dump_uint64(&self->stream, _timestamp)); \
	codepoint_t _cp; \
	PROPAGATE(GET_CP_FUNC(self, OBJ, &_cp)); \
	PROPAGATE(swriter_dump_uint16(&self->stream, _cp))

static errcode_t _tracer_timeindex_dump(tracer_t * self, usec_t timestamp, off_t offset)
{
	if (timestamp < self->next_timestamp) {
		swriter_t stream;
		char buffer[16];
		PROPAGATE(swriter_init(&stream, buffer, sizeof(buffer)));
		self->next_timestamp = timestamp + TRACER_TIMEINDEX_INTERVAL;
		DUMP_UI64(&stream, (uint64_t)timestamp);
		DUMP_UI64(&stream, (uint64_t)offset);
		PROPAGATE(listfile_append(&self->timeindex, swriter_get_buffer(&self->stream),
				swriter_get_length(&self->stream), NULL));
		PROPAGATE(swriter_fini(&stream));
	}
	RETURN_SUCCESSFUL;
}

#define RECORD_FINALIZE \
	off_t _offset; \
	PROPAGATE(rotrec_write(&self->records, swriter_get_buffer(&self->stream), \
				swriter_get_length(&self->stream), &_offset)); \
	PROPAGATE(_tracer_timeindex_dump(self, _timestamp, _offset)); \
	RETURN_SUCCESSFUL

errcode_t tracer_log(tracer_t * self, PyObject * fmtstr, PyObject * argstuple)
{
	int i;
	int count = PyTuple_GET_SIZE(argstuple);
	PyObject * item;

	RECORD_HEADER(TRACER_RECORD_LOG, _tracer_get_logline_codepoint, fmtstr);
	DUMP_UI16(&self->stream, count);
	for (i = 0; i < count; i++) {
		item = PyTuple_GET_ITEM(argstuple, i);
		PROPAGATE(_tracer_dump_obj_str(&self->stream, item, -1));
	}
	RECORD_FINALIZE;
}

errcode_t tracer_pyfunc_call(tracer_t * self, PyCodeObject * code, int argcount, PyObject * args[])
{
	RECORD_HEADER(TRACER_RECORD_PYCALL, _tracer_get_codeobj_codepoint, code);
	DUMP_UI16(&self->stream, argcount);
	int i;
	for (i = 0; i < argcount; i++) {
		PROPAGATE(_tracer_dump_argument(self, args[i]));
	}

	self->depth += 1;
	RECORD_FINALIZE;
}

errcode_t tracer_pyfunc_return(tracer_t * self, PyCodeObject * code, PyObject * retval)
{
	self->depth -= 1;
	RECORD_HEADER(TRACER_RECORD_PYRET, _tracer_get_codeobj_codepoint, code);
	PROPAGATE(_tracer_dump_argument(self, retval));
	RECORD_FINALIZE;
}

errcode_t tracer_pyfunc_raise(tracer_t * self, PyCodeObject * code,
		PyObject * exctype)
{
	if (exctype == NULL) {
		return ERR_TRACER_NO_EXCEPTION_SET;
	}

	RECORD_HEADER(TRACER_RECORD_PYRAISE, _tracer_get_codeobj_codepoint, code);
	//PROPAGATE(_tracer_dump_exception(self, exctype));
	RECORD_FINALIZE;
}

errcode_t tracer_cfunc_call(tracer_t * self, PyCFunctionObject * func)
{
	RECORD_HEADER(TRACER_RECORD_CCALL, _tracer_get_cfunc_codepoint, func);
	self->depth += 1;
	RECORD_FINALIZE;
}

errcode_t tracer_cfunc_return(tracer_t * self, PyCFunctionObject * func)
{
	self->depth -= 1;
	RECORD_HEADER(TRACER_RECORD_CRET, _tracer_get_cfunc_codepoint, func);
	RECORD_FINALIZE;
}

errcode_t tracer_cfunc_raise(tracer_t * self, PyCFunctionObject * func,
		PyObject * exctype)
{
	if (exctype == NULL) {
		return ERR_TRACER_NO_EXCEPTION_SET;
	}
	self->depth -= 1;

	RECORD_HEADER(TRACER_RECORD_CRAISE, _tracer_get_cfunc_codepoint, func);
	//PROPAGATE(_tracer_dump_exception(self, exctype));
	RECORD_FINALIZE;
}

