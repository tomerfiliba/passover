from distutils.core import setup, Extension


setup(
    name = "passover",
    version = "0.1",
    description = "high performane python tracer",
    py_modules = [
        'passover',
    ],
    ext_modules = [
        Extension("_passover",
            sources = [
                "lib/errors.c",
                "lib/fmap.c",
                "lib/hptime.c",
                "lib/rotdir.c",
                "lib/rotrec.c",
                "tracer/tracer.c",
                "tracer/_passover.c",
            ],
            define_macros = [
                ("FMAP_BACKGROUND_MUNMAP", None),
                ("HTABLE_COLLECT_STATS", None),
                ("HTABLE_BOOST_GETS", None),
            ],
            extra_compile_args = ["-Werror"], #, "-g", "-O0"],
        ),
    ],
    platforms = ["posix"],
    author = "tomer filiba",
    author_email = "tomerfiliba@gmail.com",
    url = "http://sebulbasvn.googlecode.com/svn/trunk/prace",
    long_description = "high performane python tracer",
)


