{
  description = "Nix flake for the ecc project";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-24.05";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs {
          inherit system;
          config = {
            allowUnfree = true;
          };
        };
        nativeBuildInputs = with pkgs; [
          coreutils
          cmake
          gnumake
          ninja
          python3
          stm32cubemx
          stm32loader
          stm32flash
          gcc-arm-embedded
          stlink
          openocd
          libudev-zero
          tio
        ];
        buildInputs =with pkgs; [
          libffi
          libxml2
        ];
      in
      {
        devShells.default = pkgs.mkShell {
          inherit nativeBuildInputs buildInputs env;
          name = "eccshell";
        };

      }
    );
}
