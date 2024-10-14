{pkgs ? import <nixpkgs> {}}:

pkgs.mkShell {
	buildInputs = with pkgs; [
		clang-tools
		meson
		ninja
		pkg-config
		ffmpeg-headless.dev
	];
}
