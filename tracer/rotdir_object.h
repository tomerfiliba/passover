#ifndef PYROTDIR_H_INCLUDED
#define PYROTDIR_H_INCLUDED

#include "python.h"
#include "../lib/rotdir.h"

typedef struct
{
	PyObject_HEAD
	int      inited;
	rotdir_t rotdir;
} RotdirObject;


extern PyTypeObject Rotdir_Type;


#endif // PYROTDIR_H_INCLUDED
