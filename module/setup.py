from distutils.core import setup, Extension


setup(
    name = "passover",
    version = "1.0",
    description = "high performane python tracer",
    py_modules = [
    ],
    ext_modules = [
        Extension("_passover",
            sources = [
                "lib/errors.c",
                "lib/fmap.c",
                "lib/hptime.c",
                "lib/htable.c",
                "lib/listfile.c",
                "lib/rotdir.c",
                "lib/rotrec.c",
                "lib/swriter.c",
                "tracer/tracer.c",
                "tracer/rotdir_object.c",
                "tracer/passover_object.c",
                "tracer/_passover.c",
            ],
            define_macros = [
                ("FMAP_BACKGROUND_MUNMAP", None),
                ("HTABLE_COLLECT_STATS", None),
                ("HTABLE_BOOST_GETS", None),
                ("TRACER_DUMP_ABSPATH", None),
            ],
            extra_compile_args = ["-Werror"], #, "-g", "-O0"],
        ),
    ],
    platforms = ["linux"],
    author = "tomer filiba",
    author_email = "tomerfiliba@gmail.com",
    url = "git://gitserver.xiv/python/passover",
    long_description = "high performane python tracer",
)


