{
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";

  outputs = { self, nixpkgs }: {
    packages.x86_64-linux.default = (nixpkgs.legacyPackages.x86_64-linux.callPackage ./package.nix {}).overrideAttrs { src = self; };
    packages.aarch64-linux.default = (nixpkgs.legacyPackages.aarch64-linux.callPackage ./package.nix {}).overrideAttrs { src = self; };

    devShells.x86_64-linux.default = nixpkgs.legacyPackages.x86_64-linux.mkShell {
      inputsFrom = [ self.packages.x86_64-linux.default ];
      packages = [ nixpkgs.legacyPackages.x86_64-linux.clang-tools ];
    };
    devShells.aarch64-linux.default = nixpkgs.legacyPackages.aarch64-linux.mkShell {
      inputsFrom = [ self.packages.aarch64-linux.default ];
      packages = [ nixpkgs.legacyPackages.aarch64-linux.clang-tools ];
    };
  };
}
