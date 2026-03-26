let
  sources = import ./npins;
  pkgs = import sources.nixpkgs { };
in
pkgs.mkShell {
  inputsFrom = [ (pkgs.callPackage ./package.nix { }) ];
  packages = [ pkgs.clang-tools ];
}
