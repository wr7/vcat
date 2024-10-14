project('dvel', 'cpp',
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
 ,'src/lexer.cpp'
]

executable('dvel'
          ,source_files
          ,dependencies: ffmpeg_deps
          ,install : true)
