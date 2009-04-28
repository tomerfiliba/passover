#include "python.h"

#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "../lib/errors.h"
#include "../lib/hptime.h"
#include "rotdir_object.h"
#include "passover_object.h"


PyObject * ErrorObject = NULL;
PyFunctionObject * _passover_logfunc = NULL;
PyCodeObject * _passover_logfunc_code = NULL;


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

	errcode_t retcode = hptime_init();
	if (IS_ERROR(retcode)) {
		PyErr_SetString(ErrorObject, errcode_get_name(retcode));
		return;
	}

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




