#include "python.h"

#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "../errors.h"
#include "tracer.h"

#define CO_PASSOVER_IGNORED_SINGLE   (0x02000000)
#define CO_PASSOVER_IGNORED_CHILDREN (0x04000000)
#define CO_PASSOVER_IGNORED_WHOLE    (CO_PASSOVER_IGNORED_SINGLE | CO_PASSOVER_IGNORED_CHILDREN)
#define CO_PASSOVER_DETAILED         (0x08000000)

static PyObject * ErrorObject = NULL;
static PyFunctionObject * _passover_logfunc = NULL;
static PyCodeObject * _passover_logfunc_code = NULL;

typedef struct
{
	PyObject_HEAD
	pid_t      pid;
	int        depth;
	int        ignore_depth;
	int        active;
	int        used;
	tracer_t   info;
} PassoverObject;


#define ERRCODE_TO_PYEXC(expr) \
	{ \
		errcode_t code = expr; \
		if (IS_ERROR(code)) { \
			if (PyErr_Occurred() == NULL) { \
				PyErr_SetString(ErrorObject, errcode_get_message(code)); \
			} \
			return -1; \
		} \
	}

static inline int _tracefunc_pycall_function(PassoverObject * self, PyFrameObject * frame)
{
	PyCodeObject * code = frame->f_code;
	int argcount = code->co_argcount;

	if (code->co_flags & CO_PASSOVER_DETAILED) {
		if (code->co_flags & CO_VARARGS) {
			argcount += 1;
		}
		if (code->co_flags & CO_VARKEYWORDS) {
			argcount += 1;
		}
		return tracer_pyfunc_enter(&self->info, code, argcount, frame->f_localsplus);
	}
	else {
		return tracer_pyfunc_enter(&self->info, code, 0, NULL);
	}
}

static inline int _tracefunc_is_call_ignored(PassoverObject * self, int flags)
{
	if (self->ignore_depth > 0) {
		// this function is already ignored
		self->ignore_depth += 1;
		return 1;
	}
	if (flags & CO_PASSOVER_IGNORED_CHILDREN) {
		// this function's children will be ignored
		self->ignore_depth = 1;
	}
	if (flags & CO_PASSOVER_IGNORED_SINGLE) {
		// this function itself is ignored
		return 1;
	}
	return 0;
}

static inline int _tracefunc_is_ret_ignored(PassoverObject * self, int flags)
{
	if (self->ignore_depth > 0) {
		// this is the return of a recursive ignored function
		self->ignore_depth -= 1;
		return 1;
	}
	if (flags & CO_PASSOVER_IGNORED_SINGLE) {
		// this is the return of an IGNORED_SINGLE, we ignore it anyway
		return 1;
	}
	return 0;
}

static inline int _tracefunc_excinfo(PassoverObject * self)
{
	int ret;
	PyObject *t, *v, *tb;
	PyObject * excinfo = NULL;

	PyErr_Fetch(&t, &v, &tb);
	if (t == NULL) {
		PyErr_SetString(ErrorObject, "internal error! tracefunc_exception "
				"called with no pending exception");
		return -1;
	}
	PyErr_NormalizeException(&t, &v, &tb);
	Py_INCREF(t); Py_INCREF(v); Py_INCREF(tb);
	excinfo = PyTuple_Pack(3, t, v, tb);
	if (excinfo == NULL) {
		Py_DECREF(t); Py_DECREF(v); Py_DECREF(tb);
	}
	PyErr_Restore(t, v, tb);

	// excinfo may be null if pack() failed
	ret = tracer_function_raise(&self->info, excinfo);
	Py_XDECREF(excinfo);
	return ret;
}

static inline int _tracefunc_pycall(PassoverObject * self, PyFrameObject * frame)
{
	PyCodeObject * code = frame->f_code;

	// ??? should logs be emitted even if a function is ignored?
	if (_tracefunc_is_call_ignored(self, code->co_flags)) {
		return 0;
	}

	if (code == _passover_logfunc_code) {
		/* this is not a normal trace call - it's the logger function
		 * the signature is (fmtstr, *args), so we know the first two
		 * items of f_localsplus exist
		 */
		ERRCODE_TO_PYEXC(tracer_log(&self->info, frame->f_localsplus[0],
				frame->f_localsplus[1]));
	}
	else {
		ERRCODE_TO_PYEXC(_tracefunc_pycall_function(self, frame));
	}

	return 0;
}

static inline int _tracefunc_pyret(PassoverObject * self, PyCodeObject * code, PyObject * arg)
{
	if (_tracefunc_is_ret_ignored(self, code->co_flags)) {
		return 0;
	}

	if (code == _passover_logfunc_code) {
		// this is the return of the logger, skip it
		return 0;
	}

	// XXX: change to PyErr_Occurred() != NULL ??
	if (arg == NULL) {
		if (code->co_flags & CO_PASSOVER_DETAILED) {
			ERRCODE_TO_PYEXC(_tracefunc_excinfo(self));
		}
		else {
			ERRCODE_TO_PYEXC(tracer_function_raise(&self->info, NULL));
		}
	}
	else {
		if (code->co_flags & CO_PASSOVER_DETAILED) {
			ERRCODE_TO_PYEXC(tracer_pyfunc_return(&self->info, arg));
		}
		else {
			ERRCODE_TO_PYEXC(tracer_pyfunc_return(&self->info, NULL));
		}
	}
	return 0;
}

static inline int _tracefunc_ccall(PassoverObject * self, PyCFunctionObject * func)
{
	if (_tracefunc_is_call_ignored(self, func->m_ml->ml_flags)) {
		return 0;
	}

	return 0;

	ERRCODE_TO_PYEXC(tracer_cfunc_enter(&self->info, func));
	return 0;
}

static inline int _tracefunc_cret(PassoverObject * self, PyCFunctionObject * func)
{
	if (_tracefunc_is_ret_ignored(self, func->m_ml->ml_flags)) {
		return 0;
	}

	return 0;

	ERRCODE_TO_PYEXC(tracer_cfunc_return(&self->info));
	return 0;
}

static inline int _tracefunc_cexc(PassoverObject * self, PyCFunctionObject * func)
{
	if (_tracefunc_is_ret_ignored(self, func->m_ml->ml_flags)) {
		return 0;
	}

	return 0;

	if (func->m_ml->ml_flags & CO_PASSOVER_DETAILED) {
		ERRCODE_TO_PYEXC(_tracefunc_excinfo(self));
	}
	else {
		ERRCODE_TO_PYEXC(tracer_function_raise(&self->info, NULL));
	}

	return 0;
}

static int _tracefunc(PassoverObject * self, PyFrameObject * frame,
        int event, PyObject * arg)
{
	if (getpid() != self->pid) {
		/* the child has forked/cloned, detach from it so as not to
		 * corrupt the file. note about python threads: they do no inherit
		 * the profiler, so we need not use gettid(); getpid() is good enough
		 */
		//passover_dealloc(self);
		self->active = 0;
		PyEval_SetProfile(NULL, NULL);
		return 0;
	}

	switch (event) {
		case PyTrace_CALL: // arg is NULL, arguments in frame->f_localsplus
			self->depth += 1;
			return _tracefunc_pycall(self, frame);

		case PyTrace_RETURN: // arg is the retval or NULL (exception )
			if (self->depth > 0) {
				self->depth -= 1;
				return _tracefunc_pyret(self, frame->f_code, arg);
			}
			return 0; // shallow return

		/*case PyTrace_EXCEPTION:
			not called when registering with PyEval_SetPorfile */

		case PyTrace_C_CALL: // arg is the function object
			self->depth += 1;
			return _tracefunc_ccall(self, (PyCFunctionObject*)arg);

		case PyTrace_C_RETURN: // arg is the function object
			if (self->depth > 0) {
				self->depth -= 1;
				return _tracefunc_cret(self, (PyCFunctionObject*)arg);
			}
			return 0; // shallow return

		case PyTrace_C_EXCEPTION: // arg is the function object
			if (self->depth > 0) {
				self->depth -= 1;
				return _tracefunc_cexc(self, (PyCFunctionObject*)arg);
			}
			return 0; // shallow return

		default:
			/* unexpected event -- shouldn't happen but there's nothing
			 * meaningful to do about it, so we simply ignore it. */
			return 0;
	}
}

/***************************************************************************
**                           Passover methods
***************************************************************************/

static PyObject * passover_new(PyTypeObject *type, PyObject * args, PyObject * kw)
{
	static char * kwlist[] = {"filename_prefix", "codepoints_filename", NULL};
	char * filename_prefix = NULL;
	char * codepoints_filename = NULL;
	PassoverObject * self = NULL;

	if (!PyArg_ParseTupleAndKeywords(args, kw, "ss:Passover", kwlist,
	        &tracetree_filename, codepoints_filename)) {
		return NULL;
	}

	self = (PassoverObject*)type->tp_alloc(type, 0);
	if (self == NULL) {
		return NULL;
	}

	self->active = 0;
	self->pid = getpid();
	self->depth = 0;
	self->ignore_depth = 0;
	self->used = 0;

	errcode_t retcode = tracer_init(&self->info, tracetree_filename, codepoints_filename);

	if (IS_ERROR(retcode)) {
		Py_DECREF(self);
		PyErr_SetString(ErrorObject, errcode_get_name(retcode));
		return NULL;
	}

	return (PyObject *)self;
}

PyDoc_STRVAR(passover_doc, "\
Passover(filename_prefix, codepoints_filename)\n\
\n\
\n\
\n\
\n\
\n\
\n\
\n\
");

static inline int _passover_clear(PassoverObject * self)
{
	if (self->active) {
		self->active = 0;
		PyEval_SetProfile(NULL, NULL);
	}
	return tracer_fini(&self->info);
}

static void passover_dealloc(PassoverObject * self)
{
	(void)(_passover_clear(self));
	self->ob_type->tp_free((PyObject*)self);
}

static PyObject * passover_start(PassoverObject * self, PyObject * noarg)
{
	if (self->used) {
		PyErr_SetString(ErrorObject, "tracer object already exhausted");
		return NULL;
	}
	self->used = 1;
	PyEval_SetProfile((Py_tracefunc)_tracefunc, (PyObject*)self);
	self->active = 1;
	Py_RETURN_NONE;
}

PyDoc_STRVAR(passover_start_doc, "\
start()\n\
    starts the tracer (can be called only once)\n");

static PyObject * passover_stop(PassoverObject * self, PyObject * noarg)
{
	if (_passover_clear(self) != 0) {
		//if (PyErr_Occurred() == NULL) {
		PyErr_SetString(ErrorObject, reporting_get_message());
		//}
		return NULL;
	}
	else {
		Py_RETURN_NONE;
	}
}

PyDoc_STRVAR(passover_stop_doc, "\
stop()\n\
    stops and finalizes the tracer; you cannot restart a stopped tracer object\n");

static PyMethodDef passover_methods[] = {
	{"start",	(PyCFunction)passover_start,
			METH_NOARGS | CO_PASSOVER_IGNORED_SINGLE, passover_start_doc},
	{"stop",	(PyCFunction)passover_stop,
			METH_NOARGS | CO_PASSOVER_IGNORED_SINGLE, passover_stop_doc},
	{NULL, NULL}
};


/***************************************************************************
**                           the Passover type
***************************************************************************/

static PyTypeObject Passover_Type = {
	PyObject_HEAD_INIT(NULL)
	0,                                      /* ob_size */
	"_passover.Passover" ,                  /* tp_name */
	sizeof(PassoverObject),                 /* tp_basicsize */
	0,                                      /* tp_itemsize */
	(destructor)passover_dealloc,           /* tp_dealloc */
	0,                                      /* tp_print */
	0,                                      /* tp_getattr */
	0,                                      /* tp_setattr */
	0,                                      /* tp_compare */
	0,                                      /* tp_repr */
	0,                                      /* tp_as_number */
	0,                                      /* tp_as_sequence */
	0,                                      /* tp_as_mapping */
	0,                                      /* tp_hash */
	0,                                      /* tp_call */
	0,                                      /* tp_str */
	0,                                      /* tp_getattro */
	0,                                      /* tp_setattro */
	0,                                      /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
	passover_doc,                           /* tp_doc */
	0,                                      /* tp_traverse */
	0,                                      /* tp_clear */
	0,                                      /* tp_richcompare */
	0,                                      /* tp_weaklistoffset */
	0,                                      /* tp_iter */
	0,                                      /* tp_iternext */
	passover_methods,                       /* tp_methods */
	0,                                      /* tp_members */
	0,                                      /* tp_getset */
	0,                                      /* tp_base */
	0,                                      /* tp_dict */
	0,                                      /* tp_descr_get */
	0,                                      /* tp_descr_set */
	0,                                      /* tp_dictoffset */
	0,                                      /* tp_init */
	PyType_GenericAlloc,                    /* tp_alloc */
	passover_new,                           /* tp_new */
	PyObject_Del,                           /* tp_free */
};

/***************************************************************************
**                           module functions
***************************************************************************/

static PyObject * passover_set_code_flags(PyObject * self, PyObject * args)
{
	PyCodeObject * codeobj;
	int flags;

	if (!PyArg_ParseTuple(args, "O!i:_set_flags", &PyCode_Type, &codeobj, &flags)) {
		return NULL;
	}
	codeobj->co_flags |= flags;
	Py_RETURN_NONE;
}

PyDoc_STRVAR(passover_set_code_flags_doc, "\
_set_code_flags(codeobj, flags)\n\
    updates the flags of the given code object");

static PyObject * passover_set_builtin_flags(PyObject * self, PyObject * args)
{
	PyCFunctionObject * cfunc;
	int flags;

	if (!PyArg_ParseTuple(args, "O!i:_set_flags", &PyCFunction_Type, &cfunc, &flags)) {
		return NULL;
	}
	cfunc->m_ml->ml_flags |= flags;
	Py_RETURN_NONE;
}

PyDoc_STRVAR(passover_set_builtin_flags_doc, "\
_set_builtin_flags(codeobj, flags)\n\
    updates the flags of the given builtin function object\n");

static PyObject * passover_clear_code_flags(PyObject * self, PyObject * args)
{
	PyCodeObject * codeobj;
	int flags;

	if (!PyArg_ParseTuple(args, "O!i:_set_flags", &PyCode_Type, &codeobj, &flags)) {
		return NULL;
	}
	codeobj->co_flags &= ~flags;
	Py_RETURN_NONE;
}

PyDoc_STRVAR(passover_clear_code_flags_doc, "\
_clear_code_flags(codeobj, flags)\n\
    clears the flags of the given code object\n");

static PyObject * passover_clear_builtin_flags(PyObject * self, PyObject * args)
{
	PyCFunctionObject * cfunc;
	int flags;

	if (!PyArg_ParseTuple(args, "O!i:_set_flags", &PyCFunction_Type, &cfunc, &flags)) {
		return NULL;
	}
	cfunc->m_ml->ml_flags &= ~flags;
	Py_RETURN_NONE;
}

PyDoc_STRVAR(passover_clear_builtin_flags_doc, "\
_clear_builtin_flags(codeobj, flags)\n\
    clears the flags of the given builtin function object\n");

static PyMethodDef moduleMethods[] = {
	{"_set_code_flags", (PyCFunction)passover_set_code_flags,
			METH_VARARGS, passover_set_code_flags_doc},
	{"_set_builtin_flags", (PyCFunction)passover_set_builtin_flags,
			METH_VARARGS, passover_set_builtin_flags_doc},
	{"_clear_code_flags", (PyCFunction)passover_clear_code_flags,
			METH_VARARGS, passover_clear_code_flags_doc},
	{"_clear_builtin_flags", (PyCFunction)passover_clear_builtin_flags,
			METH_VARARGS, passover_clear_builtin_flags_doc},
	{NULL, NULL}
};

/***************************************************************************
**                           module init
***************************************************************************/

PyMODINIT_FUNC init_passover(void)
{
	PyObject * module;
	module = Py_InitModule3("_passover", moduleMethods, "High performance tracer");
	if (module == NULL)
		return;

	if (ErrorObject == NULL) {
		ErrorObject = PyErr_NewException("_passover.error", NULL, NULL);
		if (ErrorObject == NULL) {
			return;
		}
	}
	Py_INCREF(ErrorObject);
	PyModule_AddObject(module, "error", ErrorObject);

	PyModule_AddIntConstant(module, "CO_PASSOVER_IGNORED_SINGLE", CO_PASSOVER_IGNORED_SINGLE);
	PyModule_AddIntConstant(module, "CO_PASSOVER_IGNORED_CHILDREN", CO_PASSOVER_IGNORED_CHILDREN);
	PyModule_AddIntConstant(module, "CO_PASSOVER_IGNORED_WHOLE", CO_PASSOVER_IGNORED_WHOLE);
	PyModule_AddIntConstant(module, "CO_PASSOVER_DETAILED", CO_PASSOVER_DETAILED);

	if (PyType_Ready(&Passover_Type) < 0) {
		return;
	}
	PyModule_AddObject(module, "Passover", (PyObject*) &Passover_Type);

	Py_XDECREF(_passover_logfunc);
	_passover_logfunc = NULL;

	PyObject * dict = PyDict_New();
	if (dict != NULL) {
		PyObject * code = Py_CompileString("def log(fmtstr, *args):\n  pass\n",
				__FILE__, Py_file_input);
		if (code != NULL) {
			PyObject * res = PyEval_EvalCode((PyCodeObject*)code, dict, dict);
			if (res != NULL) {
				_passover_logfunc = (PyFunctionObject*)PyDict_GetItemString(dict, "log");
				if (_passover_logfunc != NULL) {
					Py_INCREF(_passover_logfunc);
					PyDict_Clear(dict);
				}
				Py_DECREF(res);
			}
			Py_DECREF(code);
		}
		Py_DECREF(dict);
	}
	if (_passover_logfunc == NULL) {
		PyErr_SetString(ErrorObject, "failed to initialize log func");
		return;
	}

	PyModule_AddObject(module, "log", (PyObject*)_passover_logfunc);

	// this is only a cache, so we don't incref it
	_passover_logfunc_code = (PyCodeObject*)(_passover_logfunc->func_code);
}




