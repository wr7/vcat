project('vcat', 'cpp',
  version : '0.1',
  meson_version : '>=1.6.1',
  default_options : ['warning_level=3', 'cpp_std=c++20'])

vcat_cli = subproject('vcat_cli')

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

gawk = find_program('gawk')

shared_rs = vcat_cli.get_variable('shared_rs')

shared_hh = custom_target('shared_hh'
 ,output  : 'raw_shared.hh'
 ,input   : ['codegen/shared_to_cpp.awk', shared_rs]
 ,command : [gawk, '-f', '@INPUT0@', '--', '@INPUT1@']
 ,capture : true
)

source_files = [
  'src/main.cpp'
 ,'src/eval/builtins.cpp'
 ,'src/eval/eobject.cpp'
 ,'src/eval/eval.cpp'
 ,'src/eval/scope.cpp'
 ,'src/filter/concat.cpp'
 ,'src/filter/filter.cpp'
 ,'src/filter/params.cpp'
 ,'src/filter/util.cpp'
 ,'src/filter/video_file.cpp'
 ,'src/util.cpp'
 ,'src/util/base32.cpp'
 ,'src/error.cpp'
 ,'src/lexer.cpp'
 ,'src/lexer/token.cpp'
 ,'src/muxing.cpp'
 ,'src/muxing/util.cpp'
 ,'src/parser.cpp'
 ,'src/parser/expression.cpp'
 ,'src/parser/util.cpp'
 ,shared_hh
]

vcat_cli_dep = dependency('vcat_cli-0.1-rs')

executable('vcat'
          ,source_files
          ,dependencies: ffmpeg_deps + [vcat_cli_dep]
          ,install : true)
