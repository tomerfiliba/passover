#ifndef TRACEFUNC_H_INCLUDED
#define TRACEFUNC_H_INCLUDED

#include "python.h"
#include "tracer.h"
#include "passover_object.h"

int tracefunc(PassoverObject * self, PyFrameObject * frame,
        int event, PyObject * arg);


#endif // TRACEFUNC_H_INCLUDED
