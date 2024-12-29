{
	pkgs ? import <nixpkgs> {},

	# ensures that gdb has access to ffmpeg's debug info and source code
	# NOTE: this will cause nix to start a local build of ffmpeg
	debug ? false
}:

let

	min_meson_version = "1.6.1";

	# We need a version of Meson that may not be available in `nixpkgs` yet
	meson = if (pkgs.lib.versionAtLeast pkgs.meson.version min_meson_version)
		then pkgs.meson
		else pkgs.meson.overrideAttrs (final: old: rec {
			version = min_meson_version;
			src = pkgs.fetchFromGitHub {
				owner = "mesonbuild";
				repo = "meson";
				tag = version;
				hash = "sha256-t0JItqEbf2YqZnu5mVsCO9YGzB7WlCfsIwi76nHJ/WI=";
			};
		})
	;

in pkgs.mkShellNoCC {
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
