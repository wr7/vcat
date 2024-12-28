project('vcat', 'cpp',
  version : '0.1',
  meson_version : '>=1.5.0',
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
 ,'src/filter/util.cpp'
 ,'src/util.cpp'
 ,'src/error.cpp'
 ,'src/lexer.cpp'
 ,'src/lexer/token.cpp'
 ,'src/muxing.cpp'
 ,'src/parser.cpp'
 ,'src/parser/expression.cpp'
 ,'src/parser/util.cpp'
]

vcat_cli_dep = dependency('vcat_cli-0.1-rs')

executable('vcat'
          ,source_files
          ,dependencies: ffmpeg_deps + [vcat_cli_dep]
          ,install : true)
