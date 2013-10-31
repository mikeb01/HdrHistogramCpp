import os

env = Environment()
env['ENV']['TERM'] = os.environ['TERM']
env["CXX"] = "clang++"
env["CPPPATH"] = []
env["CPPFLAGS"] = ['-std=c++11', '-g']
#env["CPPFLAGS"] += ['-stdlib=libc++']
#env["LINKFLAGS"] = ['-stdlib=libc++']

lib = env.Clone()
lib.StaticLibrary('libUnitTest++', Glob('lib/test/UnitTest++/src/*.cpp') +
                                   Glob('lib/test/UnitTest++/src/Posix/*.cpp'))

bin = env.Clone()
bin["CPPDEFINES"] = ['__LZCNT__']
library = bin.StaticLibrary('histogram', 'src/histogram.cc')

tst = env.Clone()
tst["CPPPATH"] = ['lib/test/UnitTest++/src', 'src']
tst["LIBS"] = ['UnitTest++', 'histogram']
tst["LIBPATH"] = ['.']
tst.Program('alltests', ['test/test_histogram.cc', 'test/main.cc'])
tst.Program('onetest', ['test/test_histogram.cc', 'test/run_one.cc'])
