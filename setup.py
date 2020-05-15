#!/usr/bin/env python

import sys

if sys.version_info[0] >= 3:
    import builtins
else:
    import __builtin__ as builtins
builtins._HYPERION_SETUP_ = True

from setuptools import setup, Extension, find_packages
from distutils.command.sdist import sdist
from distutils.command.build_py import build_py
from distutils.command.build_ext import build_ext

from hyperion.testing.helper import HyperionTest
from hyperion.version import __version__


class custom_sdist(sdist):

    user_options = sdist.user_options + [('unstable', None, "make an unstable release (keep __dev__=True)")]

    def __init__(self, *args, **kwargs):
        sdist.__init__(self, *args, **kwargs)
        self.unstable = False

    def run(self):
        if not self.unstable:
            version_file = 'hyperion/version.py'
            content = open(version_file, 'r').read()
            open(version_file, 'w').write(content.replace('__dev__ = True', "__dev__ = False"))
        try:
            sdist.run(self)
        finally:
            if not self.unstable:
                open(version_file, 'w').write(content)


class NumpyBuildExt(build_ext):
    def run(self):
        import numpy
        self.include_dirs.append(numpy.get_include())
        build_ext.run(self)


cmdclass = {}
cmdclass['build_py'] = build_py
cmdclass['test'] = HyperionTest
cmdclass['sdist'] = custom_sdist
cmdclass['build_ext'] = NumpyBuildExt

if 'egg_info' in sys.argv:

    ext_modules = []

else:

    ext_modules = [Extension("hyperion.util._integrate_core",
                             ['hyperion/util/_integrate_core.c'],
                             extra_compile_args=['-Wno-error=declaration-after-statement']),
                   Extension("hyperion.util._interpolate_core",
                             ['hyperion/util/_interpolate_core.c'],
                             extra_compile_args=['-Wno-error=declaration-after-statement']),
                   Extension("hyperion.importers._discretize_sph",
                             ['hyperion/importers/_discretize_sph.c'],
                             extra_compile_args=['-Wno-error=declaration-after-statement']),
                   Extension("hyperion.grid._voronoi_core",
                             ['hyperion/grid/_voronoi_core.c',
                              'hyperion/grid/voropp_wrap.cc',
                              'hyperion/grid/voro++/c_loops.cc',
                              'hyperion/grid/voro++/cell.cc',
                              'hyperion/grid/voro++/common.cc',
                              'hyperion/grid/voro++/container.cc',
                              'hyperion/grid/voro++/container_prd.cc',
                              'hyperion/grid/voro++/pre_container.cc',
                              'hyperion/grid/voro++/unitcell.cc',
                              'hyperion/grid/voro++/v_base.cc',
                              'hyperion/grid/voro++/v_compute.cc',
                              'hyperion/grid/voro++/wall.cc'],
                             extra_compile_args = ['-O2', '-Wno-error=declaration-after-statement'],
                             extra_link_args=['-lstdc++'])]

setup(version=__version__,
      scripts=['scripts/hyperion', 'scripts/hyperion2fits'],
      cmdclass=cmdclass,
      ext_modules=ext_modules)
