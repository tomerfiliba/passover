#include "rotdir_object.h"


/***************************************************************************
**                           Rotdir methods
***************************************************************************/
static PyObject * pyrotdir_new(PyTypeObject *type, PyObject * args, PyObject * kw)
{
	static char * kwlist[] = {"path", "max_files", NULL};
	char * path = NULL;
	int max_files = 0;
	RotdirObject * self = NULL;

	if (!PyArg_ParseTupleAndKeywords(args, kw, "si:Rotdir", kwlist,
	        &path, &max_files)) {
		return NULL;
	}

	self = (RotdirObject*)type->tp_alloc(type, 0);
	if (self == NULL) {
		return NULL;
	}

	self->inited = 0;
	errcode_t retcode = rotdir_init(&self->rotdir, path, max_files);

	if (IS_ERROR(retcode)) {
		Py_DECREF(self);
		PyErr_SetString(ErrorObject, errcode_get_name(retcode));
		return NULL;
	}

	self->inited = 1;
	return (PyObject *)self;
}

static void pyrotdir_dealloc(RotdirObject * self)
{
	if (self->inited) {
		self->inited = 0;
		rotdir_fini(&self->rotdir);
	}

	self->ob_type->tp_free((PyObject*)self);
}

static PyMethodDef pyrotdir_methods[] = {
	{NULL, NULL}
};

static PyMemberDef pyrotdir_members[] = {
	{"path", T_STRING_INPLACE, offsetof(RotdirObject, rotdir) + offsetof(rotdir_t, path), READONLY,
	 PyDoc_STR("The rotdir path")},
	{"max_files", T_INT, offsetof(RotdirObject, rotdir) + offsetof(rotdir_t, max_files), READONLY,
	 PyDoc_STR("The rotdir max_files")},
	{0}
};

PyDoc_STRVAR(pyrotdir_doc, "Rotdir(path, max_files)");

PyTypeObject Rotdir_Type = {
	PyObject_HEAD_INIT(NULL)
	0,                                      /* ob_size */
	"_passover.Rotdir" ,                    /* tp_name */
	sizeof(RotdirObject),                   /* tp_basicsize */
	0,                                      /* tp_itemsize */
	(destructor)pyrotdir_dealloc,           /* tp_dealloc */
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
	pyrotdir_doc,                           /* tp_doc */
	0,                                      /* tp_traverse */
	0,                                      /* tp_clear */
	0,                                      /* tp_richcompare */
	0,                                      /* tp_weaklistoffset */
	0,                                      /* tp_iter */
	0,                                      /* tp_iternext */
	pyrotdir_methods,                       /* tp_methods */
	pyrotdir_members,                       /* tp_members */
	0,                                      /* tp_getset */
	0,                                      /* tp_base */
	0,                                      /* tp_dict */
	0,                                      /* tp_descr_get */
	0,                                      /* tp_descr_set */
	0,                                      /* tp_dictoffset */
	0,                                      /* tp_init */
	PyType_GenericAlloc,                    /* tp_alloc */
	pyrotdir_new,                           /* tp_new */
	PyObject_Del,                           /* tp_free */
};




