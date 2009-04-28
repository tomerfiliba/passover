#ifndef PASSOVER_PYTHON_H_INCLUDED
#define PASSOVER_PYTHON_H_INCLUDED

#include <Python.h>
#include <frameobject.h>
#include <code.h>
#include <structmember.h>


extern PyObject * ErrorObject;
extern PyFunctionObject * _passover_logfunc;
extern PyCodeObject * _passover_logfunc_code;


#endif // PASSOVER_PYTHON_H_INCLUDED
