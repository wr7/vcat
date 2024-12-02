rm -rf ./build
meson setup build

alias b='meson compile -C build'
alias r='b && build/vcat'
alias d='b && gdb build/vcat'
