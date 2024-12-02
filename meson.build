project('vcat', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++20'])

ffmpeg_deps = [
  dependency('libswscale'),
  dependency('libavutil'),
  dependency('libavdevice'),
  dependency('libavformat'),
  dependency('libpostproc'),
  dependency('libavcodec'),
  dependency('libavfilter'),
  dependency('libswresample')
]

source_files = [
  'src/main.cpp'
 ,'src/eval/builtins.cpp'
 ,'src/eval/eobject.cpp'
 ,'src/eval/eval.cpp'
 ,'src/filter/filter.cpp'
 ,'src/util.cpp'
 ,'src/error.cpp'
 ,'src/lexer.cpp'
 ,'src/lexer/token.cpp'
 ,'src/muxing.cpp'
 ,'src/parser.cpp'
 ,'src/parser/expression.cpp'
 ,'src/parser/util.cpp'
]

hash_inc = include_directories('deps/hash-library')
hash_lib = static_library('hash_library', 'deps/hash-library/sha256.cpp')
hash_library = declare_dependency(link_with: hash_lib, include_directories: hash_inc)

executable('vcat'
          ,source_files
          ,dependencies: ffmpeg_deps + [hash_library]
          ,install : true)
