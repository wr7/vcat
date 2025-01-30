vcat (Video conCAT) is a declarative language for editing video.

🚧 Work in progress 🚧

## Currently implemented features
### Video features
- Video concatenation (via `concat` function)
- A combination of lossless editing techniques and video caching allows for faster re-render times compared to other video editing software
- Automatic transcoding, rescaling, padding, colorspace conversion, and framerate conversion
- Fixed and mixed framerate support

### Language features
- Lists (represented with square brackets)
- `let in` expressions, ie:
```nix
let
  o = vopen;
  part1 = concat(o("1.mp4"), o("2.mp4"));
  part2 = concat(o("3.mp4"), o("4.mp4"));
in
  concat(repeat(part1, 3), part2)
```

### Miscellaneous features
- Functional CLI
- Extensive error message system (similar to the one used in my other project, [wlab](https://wr7.dev/wlab/))

## Planned features
- Audio support
- Video trimming
- Lambdas and functional programming features such as `map`
- Other video/audio effects
