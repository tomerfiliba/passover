#include "passover_object.h"
#include "rotdir_object.h"
#include "tracefunc.h"



/***************************************************************************
**                           Passover methods
***************************************************************************/

static PyObject * passover_new(PyTypeObject *type, PyObject * args, PyObject * kw)
{
	static char * kwlist[] = {"rotdir", "filename_prefix", "codepoints_filename",
			"map_size", "file_size", NULL};
	PyObject * rotdirobj = NULL;
	char * filename_prefix = NULL;
	char * codepoints_filename = NULL;
	size_t map_size;
	size_t file_size;
	PassoverObject * self = NULL;

	if (!PyArg_ParseTupleAndKeywords(args, kw, "O!ssll:Passover", kwlist,
	        &Rotdir_Type, &rotdirobj, &filename_prefix, &codepoints_filename,
	        &map_size, &file_size)) {
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

	errcode_t retcode = tracer_init(&self->info, &((RotdirObject*)rotdirobj)->rotdir,
		filename_prefix, codepoints_filename, map_size, file_size);

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
	PyEval_SetProfile((Py_tracefunc)tracefunc, (PyObject*)self);
	self->active = 1;
	Py_RETURN_NONE;
}

PyDoc_STRVAR(passover_start_doc, "\
start()\n\
    starts the tracer (can be called only once)\n");

static PyObject * passover_stop(PassoverObject * self, PyObject * noarg)
{
	errcode_t retcode = _passover_clear(self);
	if (IS_ERROR(retcode)) {
		//if (PyErr_Occurred() == NULL) {
		PyErr_SetString(ErrorObject, errcode_get_name(retcode));
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

PyTypeObject Passover_Type = {
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


