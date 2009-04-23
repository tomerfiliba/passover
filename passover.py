import sys
import os
import itertools
import shutil
from contextlib import contextmanager
import _passover

PassoverError = _passover.error
log = _passover.log

class ThreadAlreadyTraced(PassoverError):
    pass

_rotdirs = {}
_thread_counter = itertools.count()

@contextmanager
def traced(path, max_files = 100, remove_existing_dir = True):
    if path not in _rotdirs:
        if os.path.exists(path):
            if not remove_existing_dir:
                raise ValueError("path already exists")
            if not os.path.isdir(path):
                raise ValueError("path must point to a directory")
            shutil.rmtree(path)
        os.makedirs(path)
        _rotdirs[path] = (max_files, _passover.Rotdir(path, max_files))
    
    existing_max_files, rotdir = _rotdirs[path]
    assert max_files == existing_max_files
    
    prefix = "thread-%d" % (_thread_counter.next(),)
    cpfile = os.path.join(path, "codepoints")
    po = _passover.Passover(rotdir, prefix, cpfile)
    
    po.start()
    try:
        yield po
    finally:
        po.stop()


if __name__ == "__main__":
    pass




