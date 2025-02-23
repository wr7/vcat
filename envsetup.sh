meson setup build

alias b='meson compile -C build'
alias r='b && build/vcat'
alias d='b && gdb -d "$FFMPEG_SRC" --args build/vcat'
