from __future__ import with_statement
import passover


def f(a, b, c):
    return a + b * c

def g():
    passover.log("hello there")

def h(x):
    if x > 0:
        return 1 + h(x - 1)
    else:
        return 1

def j(x, y):
    return 0 / 0

def k(x):
    try:
        j(x, x)
    except Exception:
        pass

with passover.traced("tmp"):
    f(1,2,3)
    g()
    h(3)
    k(9)
    h(4)
    g()



