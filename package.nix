{
  lib,
  stdenv,
  taglib,
  fftw,
  libebur128,
  libsndfile,
  ffmpeg,
}:

stdenv.mkDerivation {
  pname = "bitfake2";
  version = "1.7";

  src = lib.cleanSource ./.;

  buildInputs = [
    taglib
    fftw
    libebur128
    libsndfile
    ffmpeg
  ];

  installFlags = [ "PREFIX=${placeholder "out"}" ];

  meta.mainProgram = "bitf";
}
