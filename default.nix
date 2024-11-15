{pkgs ? import <nixpkgs> {}}:

pkgs.mkShellNoCC {
	buildInputs = with pkgs; [
		gcc
		clang-tools
		meson
		ninja
		pkg-config
		ffmpeg-headless.dev
	];
}
