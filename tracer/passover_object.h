#ifndef PASSOVEROBJECT_H_INCLUDED
#define PASSOVEROBJECT_H_INCLUDED

#include "python.h"
#include "tracer.h"

#define CO_PASSOVER_IGNORED_SINGLE   (0x02000000)
#define CO_PASSOVER_IGNORED_CHILDREN (0x04000000)
#define CO_PASSOVER_IGNORED_WHOLE    (CO_PASSOVER_IGNORED_SINGLE | CO_PASSOVER_IGNORED_CHILDREN)
#define CO_PASSOVER_DETAILED         (0x08000000)

#define ERRCODE_TO_PYEXC(expr) \
	{ \
		errcode_t code = expr; \
		if (IS_ERROR(code)) { \
			if (PyErr_Occurred() == NULL) { \
				PyErr_SetString(ErrorObject, errcode_get_name(code)); \
			} \
			return -1; \
		} \
	}

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

extern PyTypeObject Passover_Type;


#endif // PASSOVEROBJECT_H_INCLUDED
