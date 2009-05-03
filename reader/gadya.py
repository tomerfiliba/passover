"""
Gadya: terminal-based reader for passover traces

TODO: this should one day be implemented with urwid, that displays the source 
for the select line, supports filters, searches, expand/collapse func, etc.
"""
import sys
import os
import filestructs
import time


_records = {}
def dumper(type):
    def deco(func):
        _records[type] = func
        return func
    return deco

@dumper(filestructs.PyFuncCall)
def dump_PyFuncCall(rec):
    if rec.codepoint:
        return ">>> %s(%s)   [%s:%s]" % (rec.codepoint.name, ", ".join(repr(a) for a in rec.args), 
            rec.codepoint.filename, rec.codepoint.lineno)
    else:
        return ">>> <no codepoint>"

@dumper(filestructs.PyFuncRet)
def dump_PyFuncRet(rec):
    if rec.codepoint:
        return "<<< %s(%r)   [%s:%s]" % (rec.codepoint.name, rec.retval, 
            rec.codepoint.filename, rec.codepoint.lineno)
    else:
        return "<<< <no codepoint>"

@dumper(filestructs.PyFuncRaise)
def dump_PyFuncRaise(rec):
    if rec.codepoint:
        return "XXX %s(?)   [%s:%s]" % (rec.codepoint.name, rec.codepoint.filename, 
            rec.codepoint.lineno)
    else:
        return "XXX <no codepoint>"

@dumper(filestructs.CFuncCall)
def dump_CFuncCall(rec):
    if rec.codepoint:
        return ">>> %s(?)   [%s]" % (rec.codepoint.name, ec.codepoint.module)
    else:
        return ">>> <no codepoint>"

@dumper(filestructs.CFuncRet)
def dump_CFuncRet(rec):
    if rec.codepoint:
        return "<<< %s(?)   [%s]" % (rec.codepoint.name, ec.codepoint.module)
    else:
        return "<<< <no codepoint>"

@dumper(filestructs.CFuncRaise)
def dump_CFuncRaise(rec):
    if rec.codepoint:
        return "XXX %s(?)   [%s]" % (rec.codepoint.name, ec.codepoint.module)
    else:
        return "XXX <no codepoint>"

@dumper(filestructs.LogRecord)
def dump_LogRecord(rec):
    if rec.codepoint:
        return "LOG %s" % (rec.codepoint.format % rec.args)
    else:
        return "LOG <no codepoint>"

def dump(rec):
    t = time.strftime("%m/%d %H:%M:%S", time.localtime(rec.timestamp))
    rectext = _records[type(rec)](rec)
    print "%s %s%s" % (t, "  " * rec.depth, rectext)


if __name__ == "__main__":
    #sys.argv = ["foo", "../test/tmp", "thread-0"]
    try:
        path, prefix  = sys.argv[1:]
    except ValueError:
        sys.exit("Usage: gadya /local/tlib/passover thread-0")
    #all_files = os.listdir(path)
    #traces = sorted(set(fn.rsplit(".", 2)[0] for fn in all_files if fn.endswith(".rot")))
    index = prefix.rsplit("-", 1)[1]
    cpfile = os.path.join(path, "codepoints-%s" % (index,))
    reader = filestructs.TraceReader(path, prefix, cpfile)
    for rec in reader:
        dump(rec)

