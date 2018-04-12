# setup_libics.py
from distutils.core import setup, Extension

libics_module = Extension('_libics', sources=['libics_read.c',
'libics_util.c',
'libics_write.c',
'libics_compress.c',
'libics_data.c',
'libics_test.c',
'libics_history.c',
'libics_top.c',
'libics_sensor.c',
'libics_binary.c',
'libics_gzip.c',
                                              'libics.i'], libraries=['z'])

setup(name='libics', ext_modules=[libics_module], py_modules=["libics"])
