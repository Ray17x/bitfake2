let
  sources = import ./npins;
  pkgs = import sources.nixpkgs { };
in
pkgs.callPackage ./package.nix { }
