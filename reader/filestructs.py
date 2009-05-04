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
    oldest
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
    def __init__(self, path, prefix, rot_file_size = 100 * 1024 * 1024):
        self.path = path
        self.rot_file_size = rot_file_size
        self.prefix = prefix
        self.cpfile = prefix + ".codepoints"
        self.tifile = prefix + ".timeindex"
        
        #
        # BUG BUG BUG!! the index is per the entire directory, so if you
        # have more than one thread, you'll have gaps, i.e.
        # thread-0.000001.rot
        # thread-1.000002.rot
        # thread-0.000003.rot
        # 
        # fix: the first record in the file must hold the start_offset --
        # or (file_index, file_size).we can calculate index the files either 
        # way. see _rotrec_open_window
        #
        
        files = sorted(
            (int(fn[len(prefix):].split(".")[0]), fn)
            for fn in os.listdir(self.path) 
                if fn.startswith(self.prefix) and fn.endswith(".rot")
        )
        self.oldest_file_index = files[0][0]
        self.latest_file_index = files[-1][0]
        self.files = dict(files)
        self.codepoints = self._load_codepoints()
        self.timeindex = self._load_timeindex()
        self.currfile = None
        self.currindex = None

    def _load_codepoints(self):
        codepoints = []
        for data in recfile_reader(open(os.path.join(path, self.cpfile), "rb")):
            try:
                cp = CodepointRecord.load(data)
            except EOFError:
                break
            codepoints.append(cp)
        return codepoints

    def _load_timeindex(self):
        f = open(os.path.join(path, self.tifile), "rb")
        timeindex = []
        while True:
            data = f.read(TIMEINDEX_RECORD.size)
            if len(data) != TIMEINDEX_RECORD.size:
                break
            timestamp, offset = TIMEINDEX_RECORD.unpack(data)
            timeindex.append((timestamp, offset))
        f.close()
        return timeindex
    
    def _seek_to_file_index(self, index):
        """
        if the given file exists, select it and return True. if it does not
        exist, it means it has already been rotated -- so select oldest existing
        file and return False
        """
        if index < self.oldest_file_index:
            index = self.oldest_file_index
            found = False
        elif index > self.latest_file_index:
            index = self.latest_file_index
            found = False
        else:
            found = True
        self.currfile = open(self.files[oldest_index], "rb")
        self.currindex = index
        return found
    
    def _iter_trace_files(self):
        if self.currfile is None:
            self._seek_to_file_index(0)
        while self.currindex <= self.latest_file_index:
            self._seek_to_file_index(self.currindex + 1)
            yield self.currfile

    def _iter_currfile_records(self):
        for data in recfile_reader(self.currfile):
            try:
                rec = TraceRecord.load(data)
            except EOFError:
                break
            else:
                rec._codepoints = self.codepoints
                yield rec
    
    def __iter__(self):
        for file in self._iter_trace_files():
            for record in self._iter_currfile_records():
                yield record

    #
    # APIs
    #
    def seek_to_offset(self, offset):
        file_index = offset // self.rot_file_size
        in_file_offset = offset % self.rot_file_size
        if self._seek_to_file_index(file_index):
            self.currfile.seek(in_file_offset)

    def seek_to_timestamp(self, timestamp):
        timestamp, offset = binary_search(self.timeindex, 
            lambda obj: cmp(obj[0], timestamp))
        self.seek_to_offset(offset)
    
    def read(self):
        raise NotImplementedError()


if __name__ == "__main__":
    reader = TraceReader("../test/tmp", "thread-0", "../test/tmp/codepoints-0")
    for rec in reader:
        print rec, rec.codepoint



