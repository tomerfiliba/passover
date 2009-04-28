#include "tracefunc.h"


#define ERRCODE_TO_PYEXC(__expr) \
	{ \
		errcode_t __code = __expr; \
		if (IS_ERROR(__code)) { \
			if (PyErr_Occurred() == NULL) { \
				PyErr_SetString(ErrorObject, errcode_get_name(__code)); \
			} \
			return -1; \
		} \
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

static inline errcode_t _tracefunc_pycall_function(PassoverObject * self, PyFrameObject * frame)
{
	PyCodeObject * code = frame->f_code;
	int argcount = code->co_argcount;

	if (code->co_flags & CO_VARARGS) {
		argcount += 1;
	}
	if (code->co_flags & CO_VARKEYWORDS) {
		argcount += 1;
	}
	return tracer_pyfunc_call(&self->info, code, argcount, frame->f_localsplus);
	//if (code->co_flags & CO_PASSOVER_DETAILED) {
}

static inline int _tracefunc_pycall(PassoverObject * self, PyFrameObject * frame)
{
	PyCodeObject * code = frame->f_code;

	//printf("PYCALL\n");

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

static inline int _tracefunc_pyret(PassoverObject * self, PyCodeObject * code, PyObject * retval)
{
	//printf("PYRET\n");
	if (_tracefunc_is_ret_ignored(self, code->co_flags)) {
		return 0;
	}

	if (code == _passover_logfunc_code) {
		// this is the return of the logger, skip it
		return 0;
	}

	//if (code->co_flags & CO_PASSOVER_DETAILED) {
	ERRCODE_TO_PYEXC(tracer_pyfunc_return(&self->info, code, retval));
	return 0;
}

static inline int _tracefunc_pyexc(PassoverObject * self, PyCodeObject * code, PyObject * excval)
{
	//printf("PYEXC\n");
	if (_tracefunc_is_ret_ignored(self, code->co_flags)) {
		return 0;
	}

	//if (code->co_flags & CO_PASSOVER_DETAILED) {
	ERRCODE_TO_PYEXC(tracer_pyfunc_raise(&self->info, code, excval));
	return 0;
}

static inline int _tracefunc_ccall(PassoverObject * self, PyCFunctionObject * func)
{
	//printf("CCALL\n");

	if (_tracefunc_is_call_ignored(self, func->m_ml->ml_flags)) {
		return 0;
	}

	ERRCODE_TO_PYEXC(tracer_cfunc_call(&self->info, func));
	return 0;
}

static inline int _tracefunc_cret(PassoverObject * self, PyCFunctionObject * func)
{
	//printf("CRET\n");

	if (_tracefunc_is_ret_ignored(self, func->m_ml->ml_flags)) {
		return 0;
	}

	ERRCODE_TO_PYEXC(tracer_cfunc_return(&self->info, func));
	return 0;
}

static inline int _tracefunc_cexc(PassoverObject * self, PyCFunctionObject * func, PyObject * excval)
{
	//printf("CEXC\n");

	if (_tracefunc_is_ret_ignored(self, func->m_ml->ml_flags)) {
		return 0;
	}

	//if (func->m_ml->ml_flags & CO_PASSOVER_DETAILED) {
	ERRCODE_TO_PYEXC(tracer_cfunc_raise(&self->info, func, excval));
	return 0;
}

#define TSTATE_EXCTYPE(frame) frame->f_tstate->exc_type ? frame->f_tstate->exc_type : frame->f_tstate->curexc_type

int tracefunc(PassoverObject * self, PyFrameObject * frame,
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
				if (arg == NULL) {
					return _tracefunc_pyexc(self, frame->f_code, TSTATE_EXCTYPE(frame));
				}
				else {
					return _tracefunc_pyret(self, frame->f_code, arg);
				}
			}
			return 0; // shallow return

		/*
		case PyTrace_EXCEPTION:
			// not called when registering with PyEval_SetProfile
		*/

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
				return _tracefunc_cexc(self, (PyCFunctionObject*)arg, TSTATE_EXCTYPE(frame));
			}
			return 0; // shallow return

		default:
			/* unexpected event -- shouldn't happen but there's nothing
			 * meaningful to do about it, so we simply ignore it. */
			return 0;
	}
}

