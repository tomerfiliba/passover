"""
passover filestructs: reading the codepoints file and the traces
"""
import os
from cStringIO import StringIO
from struct import Struct, error as StructError


UINT8 = Struct("=B")
UINT16 = Struct("=H")
UINT32 = Struct("=L")
UINT64 = Struct("=Q")
TIMEINDEX_RECORD = Struct("=QQ")

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
        self.name = self.read_str(stream)

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

def bisect(data, value, keyfunc = lambda obj: obj, lo = 0, hi = None):
    if hi is None:
        hi = len(a)
    key = keyfunc(value)
    while lo < hi:
        mid = (lo + hi) // 2
        mkey = keyfunc(data[mid])
        if mkey < key:
            lo = mid + 1
        elif mkey > key: 
            hi = mid
    return lo

class RotdirFile(object):
    __slots__ = ["file", "index", "min_offset", "max_offset"]
    
    def __init__(self, file, index, min_offset, max_offset):
        self.file = file
        self.index = index
        self.min_offset = min_offset
        self.max_offset = max_offset
    
    def __contains__(self, offset):
        return self.min_offset <= offset < self.max_offset

    def seek(self, offset):
        self.file.seek(offset - self.min_offset)
    
    def read(self, count):
        return self.file.read(count)

ROTREC_HEADER = UINT64

class RotdirReader(object):
    __slots__ = ["path", "files", "min_offset", "max_offset", "curr_file", "curr_offset"]
    
    def __init__(self, path, prefix):
        self.path = path
        files = [fn for fn in os.listdir(self.path) 
            if fn.startswith(prefix) and fn.endswith(".rot")]
        self.files = []
        for fn in files:
            fn = os.path.join(self.path, fn)
            data = open(fn, "rb").read(ROTREC_HEADER.size)
            if len(data) != ROTREC_HEADER.size:
                break
            base_offset, = ROTREC_HEADER.unpack(data)
            self.files.append((base_offset, fn))
        if not self.files:
            raise ValueError("")
        self.files.sort(key = lambda obj: obj[0])
        self.min_offset = self.files[0][0]
        last_base, last_fn = self.files[-1]
        self.max_offset = last_base + os.stat(last_fn).st_size
        self.curr_file = None
        self.curr_offset = None
    
    def _get_file_index(self, offset):
        if offset < self.min_offset or offset > self.max_offset:
            raise ValueError("offset %r is out of bounds [%r, %r)" % (offset, 
                self.min_offset, self.max_offset))
        i = bisect(self.files, [offset], keyfunc = lambda obj: obj[0])
        assert i > 0
        return i - 1

    def _select(self, index):
        base, fn = self.files[index]
        if index == len(self.files) - 1:
            max = self.max_offset
        else:
            max, _ = self.files[index + 1]
        file = open(fn, "rb")
        file.seek(ROTREC_HEADER.size)
        self.curr_file = RotdirFile(file, index, base, max)
    
    def _select_next(self):
        if self.curr_file.index + 1 > len(self.files):
            raise EOFError()
        self._select(self.curr_file.index + 1)
    
    def seek(self, offset):
        self._select(self._get_file_index(offset))
        self.curr_file.seek(offset)
    
    def _read_record(self):
        try:
            length, = UINT16.unpack(self.curr_file.read(UINT16.size))
        except StructError:
            raise EOFError()
        data = self.curr_file.read(length)
        #if len(data) != length:
        #    raise EOFError()
        return data
    
    def read_record(self):
        if self.curr_file is None:
            self._select(0)
        try:
            return self._read_record()
        except EOFError:
            self._select_next()
            return self._read_record()

class TraceReader(object):
    __slots__ = ["rotdir", "codepoints", "timeindex"]
    def __init__(self, path, prefix):
        self.rotdir = RotdirReader(path, prefix)
        self.codepoints = self._load_codepoints(os.path.join(path, prefix + ".codepoints"))
        self.timeindex = self._load_timeindex(os.path.join(path, prefix + ".timeindex"))

    @classmethod
    def _load_codepoints(cls, filename):
        codepoints = []
        for data in recfile_reader(open(filename, "rb")):
            try:
                cp = CodepointRecord.load(data)
            except EOFError:
                break
            codepoints.append(cp)
        return codepoints

    @classmethod
    def _load_timeindex(cls, filename):
        f = open(filename, "rb")
        timeindex = []
        while True:
            data = f.read(TIMEINDEX_RECORD.size)
            if len(data) != TIMEINDEX_RECORD.size:
                break
            timestamp, offset = TIMEINDEX_RECORD.unpack(data)
            timeindex.append((timestamp, offset))
        f.close()
        return timeindex
    
    #
    # APIs
    #
    def seek_to_offset(self, offset):
        self.rotdir.seek(offset)
    
    def seek_to_timestamp(self, timestamp):
        i = bisect(self.timeindex, [timestamp], keyfunc = lambda obj: obj[0])
        assert i > 0
        ts, offset = self.timeindex[i - 1]
        self.seek_to_offset(offset)
    
    def read(self):
        data = self.rotdir.read_record()
        rec = TraceRecord.load(data)
        rec._codepoints = self.codepoints
        return rec
    
    def __iter__(self):
        try:
            while True:
                yield self.read()
        except EOFError:
            pass

if __name__ == "__main__":
    reader = TraceReader("../test/tmp", "thread-0")
    for rec in reader:
        print rec, rec.codepoint



