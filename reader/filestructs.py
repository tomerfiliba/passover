"""
passover filestructs: reading the codepoints file and the traces
"""
import os
from StringIO import StringIO
from struct import Struct, error as StructError


UINT8 = Struct("B")
UINT16 = Struct("H")
UINT32 = Struct("L")
UINT64 = Struct("Q")

class TraceRecord(object):
    CONCRETE_RECORDS = None
    TYPE = None
    __slots__ = ["depth", "timestamp"]
    
    def __init__(self, depth, timestamp):
        self.depth = depth
        self.timestamp = timestamp
    
    @classmethod
    def read_uint8(cls, stream):
        return UINT8.unpack(stream.read(UINT8.size))[0]
    @classmethod
    def read_uint16(cls, stream):
        return UINT16.unpack(stream.read(UINT16.size))[0]
    @classmethod
    def read_uint32(cls, stream):
        return UINT32.unpack(stream.read(UINT32.size))[0]
    @classmethod
    def read_uint64(cls, stream):
        return UINT64.unpack(stream.read(UINT64.size))[0]
    @classmethod
    def read_str(cls, stream):
        length = cls.read_uint16(stream)
        return stream.read(length)

    @classmethod
    def read_header(cls, stream):
        try:
            type = cls.read_uint8(stream)
        except StructError:
            type = 0
        if type == 0:
            return None, None, None
        depth = cls.read_uint16(stream)
        timestamp = cls.read_uint64(stream)
        return type, depth, timestamp
    
    def parse(self, stream):
        raise NotImplementedError()
    
    @classmethod
    def load(cls, data):
        if not cls.CONCRETE_RECORDS:
            cls.CONCRETE_RECORDS = dict((subcls.TYPE, subcls) 
                for subcls in cls.__subclasses__() if subcls.TYPE is not None)
        stream = StringIO(data)
        #_read = stream.read
        #def read2(count):
        #    data = _read(count)
        #    print count, repr(data)
        #    return data
        #stream.read = read2
        type, depth, timestamp = cls.read_header(stream)
        if type is None:
            raise EOFError
        inst = cls.CONCRETE_RECORDS[type](depth, timestamp)
        inst.parse(stream)
        return inst

class PyFuncCall(TraceRecord):
    TYPE = 1
    __slots__ = ["filename", "name", "lineno", "args"]
    
    def parse(self, stream):
        self.filename = self.read_str(stream)
        self.name = self.read_str(stream)
        self.lineno = self.read_uint32(stream)
        argcount = self.read_uint16(stream)
        self.args = [self.read_str(stream) for i in range(argcount)]

class PyFuncRet(TraceRecord):
    TYPE = 2
    __slots__ = ["filename", "name", "lineno", "retval"]
    
    def parse(self, stream):
        self.filename = self.read_str(stream)
        self.name = self.read_str(stream)
        self.lineno = self.read_uint32(stream)
        self.retval = self.read_str(stream)

class PyFuncRaise(TraceRecord):
    TYPE = 3
    __slots__ = ["filename", "name", "lineno", "exctype"]
    
    def parse(self, stream):
        self.filename = self.read_str(stream)
        self.name = self.read_str(stream)
        self.lineno = self.read_uint32(stream)
        self.exctype = self.read_str(stream)

class CFuncCall(TraceRecord):
    TYPE = 4
    __slots__ = ["module", "name"]
    
    def parse(self, stream):
        self.module = self.read_str(stream)
        self.name = self.read_str(stream)

class CFuncRet(TraceRecord):
    TYPE = 5
    __slots__ = ["module", "name"]
    
    def parse(self, stream):
        self.module = self.read_str(stream)
        self.name = self.read_str(stream)

class CFuncRaise(TraceRecord):
    TYPE = 6
    __slots__ = ["module", "name", "exctype"]
    
    def _read(self, stream):
        self.module = self.read_str(stream)
        self.name = self.read_str(stream)
        self.exctype = self.read_str(stream)

class LogRecord(TraceRecord):
    TYPE = 7
    __slots__ = ["fmtstr", "args"]
    
    def parse(self, stream):
        self.fmtstr = self.read_str(stream)

class TraceFile(object):
    def __init__(self, file):
        self.file = file
    
    def __iter__(self):
        while True:
            length, = UINT16.unpack(self.file.read(UINT16.size))
            data = self.file.read(length)
            try:
                rec = TraceRecord.load(data)
            except EOFError:
                break
            else:
                yield rec

class TraceReader(object):
    def __init__(self, path, prefix):
        self.path = path
        self.prefix = prefix
        self.file = None
    
    def _get_trace_files(self):
        curr = None
        while True:
            files = sorted(fn for fn in os.listdir(self.path) 
                if fn.startswith(self.prefix) and fn.endswith(".rot"))
            if not files:
                break # no files at all
            if curr is None:
                curr = files[0]
            else:
                try:
                    index = files.index(curr)
                except ValueError:
                    curr = files[0]
                else:
                    if index + 1 >= len(files):
                        break # finished all files
                    else:
                        curr = files[index + 1]
            
            yield TraceFile(open(os.path.join(self.path, curr), "rb"))
    
    def __iter__(self):
        for tracefile in self._get_trace_files():
            for record in tracefile:
                yield record


if __name__ == "__main__":
    reader = TraceReader("../test/tmp", "thread-0")
    for rec in reader:
        print rec



