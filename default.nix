{
	pkgs ? import <nixpkgs> {},

	# ensures that gdb has access to ffmpeg's debug info and source code
	# NOTE: this will cause nix to start a local build of ffmpeg
	debug ? false
}:

pkgs.mkShellNoCC {
	buildInputs = with pkgs; [
		gcc
		clang-tools
		meson
		ninja
		pkg-config
		(if debug then (enableDebugging ffmpeg) else ffmpeg)
	];

	env = {
		FFMPEG_SRC = pkgs.lib.strings.optionalString debug pkgs.ffmpeg.src;
	};
}
