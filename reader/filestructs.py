"""
passover filestructs: reading the codepoints file and the traces
"""
import os
from cStringIO import StringIO
from struct import Struct, error as StructError


UINT8 = Struct("B")
UINT16 = Struct("H")
UINT32 = Struct("L")
UINT64 = Struct("Q")

class BinaryRecord(object):
    TYPE = None
    CONCRETE_RECORDS = None
    __slots__ = []
    
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
    
    def parse(self, stream):
        raise NotImplementedError()
    
    @classmethod
    def load(cls, data):
        assert not cls.TYPE
        if not cls.CONCRETE_RECORDS:
            cls.CONCRETE_RECORDS = dict((subcls.TYPE, subcls) 
                for subcls in cls.__subclasses__() if subcls.TYPE)

        stream = StringIO(data)
        try:
            type = cls.read_uint8(stream)
        except StructError, ex:
            type = 0
        if type == 0:
            raise EOFError
        inst = cls.CONCRETE_RECORDS[type]()
        inst.parse(stream)
        return inst

#===============================================================================
# codepoints
#===============================================================================
class CodepointRecord(BinaryRecord):
    pass

class LoglineCodepoint(CodepointRecord):
    TYPE = 1
    __slots__ = ["format"]
    
    def parse(self, stream):
        self.format = self.read_str(stream)

class PyFuncCodepoint(CodepointRecord):
    TYPE = 2
    __slots__ = ["filename", "name", "lineno"]
    
    def parse(self, stream):
        self.filename = self.read_str(stream)
        self.name = self.read_str(stream)
        self.lineno = self.read_uint32(stream)

class CFuncCodepoint(CodepointRecord):
    TYPE = 3
    __slots__ = ["module", "name"]
    
    def parse(self, stream):
        self.module = self.read_str(stream)
        self.name = self.read_str(name)

#===============================================================================
# trace records
#===============================================================================
class Undumpable(object):
    __slots__ = []
    def __repr__(self):
        return "Undumpable"
Undumpable = Undumpable()

class TraceRecord(BinaryRecord):
    __slots__ = ["depth", "timestamp", "cpindex", "_codepoints"]

    MIN_IMM_INT = -20
    MAX_IMM_INT = 30
    IMMINT_0 = 50    
    
    ARGUMENT_READERS = {
        0: lambda cls, stream: None,
        1: lambda cls, stream: Undumpable,
        2: lambda cls, stream: True,
        3: lambda cls, stream: False,
        4: lambda cls, stream: int(cls.read_str(stream)),
        5: lambda cls, stream: long(cls.read_str(stream)),
        6: lambda cls, stream: float(cls.read_str(stream)),
        7: lambda cls, stream: cls.read_str(stream),
    }
    for i in range(MIN_IMM_INT, MAX_IMM_INT + 1):
        ARGUMENT_READERS[IMMINT_0 + i] = (lambda cls, stream, i = i: i)
    
    def parse_header(self, stream):
        self.depth = self.read_uint16(stream)
        self.timestamp = self.read_uint64(stream) / 1000000.0 # in usecs
        self.cpindex = self.read_uint16(stream)
    
    def parse_body(self, stream):
        raise NotImplementedError()
    
    def parse(self, stream):
        self.parse_header(stream)
        self.parse_body(stream)
    
    @property
    def codepoint(self):
        try:
            return self._codepoints[self.cpindex]
        except IndexError:
            return None
    
    @classmethod
    def read_argument(cls, stream):
        type = cls.read_uint8(stream)
        return cls.ARGUMENT_READERS[type](cls, stream)
    
    def __repr__(self):
        return "%s(depth = %s, timestamp = %s, cpindex = %s)" % (
            self.__class__.__name__, self.depth, self.timestamp, self.cpindex)

class PyFuncCall(TraceRecord):
    TYPE = 1
    __slots__ = ["args"]
    
    def parse_body(self, stream):
        count = self.read_uint16(stream)
        self.args = [self.read_argument(stream) for i in range(count)]

class PyFuncRet(TraceRecord):
    TYPE = 2
    __slots__ = ["retval"]
    
    def parse_body(self, stream):
        self.retval = self.read_argument(stream)

class PyFuncRaise(TraceRecord):
    TYPE = 3
    __slots__ = ["exctype"]
    
    def parse_body(self, stream):
        #self.exctype = self.read_str(stream)
        pass

class CFuncCall(TraceRecord):
    TYPE = 4
    __slots__ = []
    
    def parse_body(self, stream):
        pass

class CFuncRet(TraceRecord):
    TYPE = 5
    __slots__ = []
    
    def parse_body(self, stream):
        pass

class CFuncRaise(TraceRecord):
    TYPE = 6
    __slots__ = ["exctype"]
    
    def parse_body(self, stream):
        #self.exctype = self.read_str(stream)
        pass

class LogRecord(TraceRecord):
    TYPE = 7
    __slots__ = ["args"]
    
    def parse_body(self, stream):
        count = self.read_uint16(stream)
        self.args = [self.read_str(stream) for i in range(count)]        

#===============================================================================
# Files
#===============================================================================
def recfile_reader(file):
    while True:
        try:
            length, = UINT16.unpack(file.read(UINT16.size))
        except StructError:
            break
        yield file.read(length)

class TraceReader(object):
    def __init__(self, path, prefix, cpfile):
        self.path = path
        self.prefix = prefix
        self.cpfile = cpfile
        self.codepoints = self.load_codepoints()

    def load_codepoints(self):
        codepoints = []
        for data in recfile_reader(open(self.cpfile, "rb")):
            try:
                cp = CodepointRecord.load(data)
            except EOFError:
                break
            codepoints.append(cp)
        return codepoints

    def tracefile_reader(self, file):
        for data in recfile_reader(file):
            try:
                rec = TraceRecord.load(data)
            except EOFError:
                break
            else:
                rec._codepoints = self.codepoints
                yield rec
    
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
            
            yield open(os.path.join(self.path, curr), "rb")
    
    def __iter__(self):
        for file in self._get_trace_files():
            for record in self.tracefile_reader(file):
                yield record


if __name__ == "__main__":
    reader = TraceReader("../test/tmp", "thread-0", "../test/tmpcodepoints-0")
    for rec in reader:
        print rec, rec.codepoint



