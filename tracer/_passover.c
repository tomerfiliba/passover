#include "python.h"

#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "../lib/errors.h"
#include "rotdir_object.h"
#include "passover_object.h"


PyObject * ErrorObject = NULL;
static PyFunctionObject * _passover_logfunc = NULL;
static PyCodeObject * _passover_logfunc_code = NULL;


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
		return tracer_pyfunc_call(&self->info, code, argcount, frame->f_localsplus);
	}
	else {
		return tracer_pyfunc_call(&self->info, code, 0, NULL);
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
	ret = tracer_raise(&self->info, excinfo);
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
			ERRCODE_TO_PYEXC(tracer_raise(&self->info, NULL));
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

	ERRCODE_TO_PYEXC(tracer_cfunc_call(&self->info, func));
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
		ERRCODE_TO_PYEXC(tracer_raise(&self->info, NULL));
	}

	return 0;
}

int _tracefunc(PassoverObject * self, PyFrameObject * frame,
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
	if (PyType_Ready(&Rotdir_Type) < 0) {
		return;
	}
	PyModule_AddObject(module, "Rotdir", (PyObject*) &Rotdir_Type);

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




