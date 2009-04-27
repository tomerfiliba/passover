from __future__ import with_statement
import passover


def f(a, b, c):
    print a, b, c
    return a + b * c

def g():
    passover.log("hello there")
    1/0

with passover.traced("/tmp/passover"):
    f(1,2,3)
    f(2,3,4)
    g()
    f(3,4,5)



