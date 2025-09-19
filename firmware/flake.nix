{
  description = "Full development environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-25.05";
    flake-utils.url = "github:numtide/flake-utils";
    esp-flake.url = "github:mirrexagon/nixpkgs-esp-dev";
  };

  outputs = {
    nixpkgs,
    flake-utils,
    esp-flake,
    ...
  }:
    flake-utils.lib.eachDefaultSystem (
      system: let
        pkgs = import nixpkgs {
            inherit system;
            overlays = [esp-flake.overlays.default];
          };
      in {
        devShells.default = pkgs.mkShell rec {
          buildInputs = with pkgs; [
            git
            wget
            # gnumake

            cmake
            neocmakelsp

            # llvm-xtensa

            # stdenv.cc.cc.lib
            esp-idf-full
            # llvm-xtensa-lib

            # flex
            # bison
            # gperf
            # libxml2
            # zlib

            # espflash
            # ldproxy
            # ncurses5

            minicom

            python3
            python3Packages.pip
            python3Packages.virtualenv
          ];

          # LD_LIBRARY_PATH = pkgs.lib.makeLibraryPath (buildInputs);

          # ;

          # ESP_IDF_VERSION = pkgs.esp-idf-esp32c6.version;

          # LIBCLANG_PATH = "${pkgs.llvm-xtensa-lib}/lib";
          shellHook = ''
            # fixes libstdc++ issues and libgl.so issues
            export LD_LIBRARY_PATH=${pkgs.lib.makeLibraryPath [pkgs.libxml2 pkgs.zlib pkgs.stdenv.cc.cc.lib]}
            export ESP_IDF_VERSION=${pkgs.esp-idf-full.version}
          '';
        };
      }
    );
}
