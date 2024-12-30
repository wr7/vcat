function process_line(line) {
  sub(/^(\t|    )/, "", line)

  if(match(line, /^([\t ]*)struct ([A-Za-z\d_]+)/, r)) {
    print r[1] "#undef STRUCT_NAME"
    print r[1] "#define STRUCT_NAME " r[2]
    print line
    print "public:"
  } else {
    print line
  }
}

{
  if(NR > 1 && prev !~ /vcat_def!\(/) {
    process_line(prev)
  }

  prev = $0
}
