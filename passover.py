from __future__ import with_statement
import sys
import os
import gc
import itertools
import thread
import shutil
from contextlib import contextmanager
import _passover

PassoverError = _passover.error

class TracerPathError(PassoverError):
    pass
class ThreadAlreadyTraced(PassoverError):
    pass
class RotdirMaxFilesMismatch(PassoverError):
    pass

#===============================================================================
# internal APIs
#===============================================================================
def _get_code(func):
    if hasattr(func, "im_func"):
        func = func.im_func
    if hasattr(func, "func_code"):
        func = func.func_code
    return func

def _set_flag(func, flag):
    # python will not allow for unknown flags... pitty
    if isinstance(func, BuiltinFunctionType):
        pass
        #_hptrace._set_builtin_flags(func, flag)
    else:
        _hptrace._set_code_flags(_get_code(func), flag)
    return func

def _clear_flag(func, flag):
    if isinstance(func, BuiltinFunctionType):
        #_hptrace._clear_builtin_flags(func, flag)
        pass
    else:
        _hptrace._clear_code_flags(_get_code(func), flag)
    return func

def _get_all_functions(codepred, bltpred):
    for obj in gc.get_objects():
        if isinstance(obj, FunctionType) and codepred(obj):
            yield obj
        elif isinstance(obj, BuiltinFunctionType) and bltpred(obj):
            yield obj

#===============================================================================
# ignore function, module and package
#===============================================================================
SINGLE = _passover.CO_PASSOVER_IGNORED_SINGLE
CHILDREN = _passover.CO_PASSOVER_IGNORED_CHILDREN
WHOLE = _passover.CO_PASSOVER_IGNORED_WHOLE
#DETAILED = _passover.CO_PASSOVER_DETAILED

def ignore_function(func, mode = CHILDREN):
    return _set_flag(func, mode)

def ignore_module(module, mode = WHOLE):
    fn = module.__file__.rsplit(".", 1)[0]
    def codepred(obj, fn = fn):
        return obj.func_code.co_filename.startswith(fn)
    def bltpred(obj, modname = module.__name__):
        return obj.__module__ == modname
    for func in _get_all_functions(bltpred, codepred):
        ignore_function(func, mode)

def ignore_package(module, mode = WHOLE):
    if hasattr(module, "__path__"):
        fn = module.__path__[0]
    elif hasattr(module, "__file__"):
        fn = module.__file__.rsplit(".", 1)[0]
    else:
        fn = None
    if fn:
        def codepred(obj, fn = fn):
            return obj.func_code.co_filename.startswith(fn)
    else:
        def codepred(obj):
            return False
    def bltpred(obj, modname = module.__name__):
        return obj.__module__ and obj.__module__.startswith(modname)
    
    for func in _get_all_functions(bltpred, codepred):
        ignore_function(func, mode)

#===============================================================================
# threading
#===============================================================================
_rotdirs = {}
_thread_counter = itertools.count()
_orig_start_new_thread = thread.start_new_thread
_orig_start_new = thread.start_new
_per_thread = thread._local()

def _thread_wrapper(func, args, kwargs):
    with _traced(_per_thread.rotdir, template = _per_thread.template, 
            cptemplate = _per_thread.cptemplate, map_size = _per_thread.map_size, 
            file_size = _per_thread.file_size):
        return func(*args, **kwargs)

def _start_new_thread(func, args, kwargs = {}):
    if _per_thread.traced and _per_thread.trace_children:
        return _orig_start_new_thread(_thread_wrapper, (func, args, kwargs))
    else:
        return _orig_start_new_thread(func, args, kwargs)

thread.start_new_thread = thread.start_new = _start_new_thread

@contextmanager
def _traced(rotdir, template, cptemplate, trace_children, map_size, file_size):
    tid = _thread_counter.next()
    _per_thread.tid = tid
    _per_thread.traced = False
    
    _per_thread.rotdir = rotdir
    _per_thread.map_size = map_size
    _per_thread.file_size = file_size
    _per_thread.template = template
    _per_thread.cptemplate = cptemplate
    _per_thread.trace_children = trace_children
    
    prefix = template % (tid,)
    cpfile = os.path.join(rotdir.path, cptemplate % (tid,))

    po = _passover.Passover(rotdir, prefix, cpfile, map_size, file_size)
    po.start()
    _per_thread.traced = False
    try:
        yield po
    finally:
        _per_thread.traced = False
        po.stop()


#===============================================================================
# tracing APIs: log and traced()
#===============================================================================
MB = 1024 * 1024

log = _passover.log

@contextmanager
def traced(path, max_files = 100, delete_path_if_exists = True, 
        template = "thread-%d", cptemplate = "codepoints-%d",
        trace_threads = True, map_size = 2 * MB, file_size = 100 * MB):
    path = os.path.abspath(path)
    if path not in _rotdirs:
        if os.path.exists(path):
            if not os.path.isdir(path):
                raise TracerPathError("path must point to a directory")
            if not delete_path_if_exists:
                raise TracerPathError("path already exists")
            shutil.rmtree(path)
        os.makedirs(path)
        _rotdirs[path] = _passover.Rotdir(path, max_files)
    
    rotdir = _rotdirs[path]
    if max_files != rotdir.max_files:
        raise RotdirMaxFilesMismatch("rotdir already exists with a different "
            "number of max_files")
    
    with _traced(rotdir, template = template, cptemplate = cptemplate, 
            trace_children = trace_threads, map_size = map_size, 
            file_size = file_size) as po:
        yield po


if __name__ == "__main__":
    # parse commandline arguments and run script traced
    pass




