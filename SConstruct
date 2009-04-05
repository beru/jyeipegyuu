import os

src_files = Split(
""" BZip2Compressor.cpp
	IntraPrediction.cpp
	Quantizer.cpp
	main.cpp
	ReadImage/File.cpp
	ReadImage/ReadImage.cpp
""")

env = Environment()
env['ENV']['CPLUS_INCLUDE_PATH'] = os.environ['CPLUS_INCLUDE_PATH']

objs = env.Object(src_files, CPPPATH=['./'])
env.Program(target = 'jyeipegyuu', source = objs, LIBS=['bz2'])
