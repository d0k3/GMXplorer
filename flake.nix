{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    devkitNix.url = "github:bandithedoge/devkitNix";
    devkitNix.inputs.nixpkgs.follows = "nixpkgs";
    hax-nur.url = "github:ihaveamac/nur-packages";
    hax-nur.inputs.nixpkgs.follows = "nixpkgs";
  };

  outputs = { self, nixpkgs, devkitNix, hax-nur }: {
    devShells.x86_64-linux.default = let
      pkgs = import nixpkgs { system = "x86_64-linux"; overlays = [ devkitNix.overlays.default ]; };
    in pkgs.mkShell {
      packages = with pkgs; [
        pkgs.devkitNix.devkitARM
        python3Packages.python
        ( python3Packages.callPackage ./firmtool.nix { } )
        lua54Packages.lua
        hax-nur.packages.x86_64-linux.ctrtool
        hax-nur.packages.x86_64-linux._3dstool
      ];

      inherit (pkgs.devkitNix.devkitARM) shellHook;
    };

    packages.x86_64-linux = let
      pkgs = import nixpkgs { system = "x86_64-linux"; overlays = [ devkitNix.overlays.default ]; };
    in rec {
      godmode9 = pkgs.stdenvNoCC.mkDerivation rec {
        pname = "GodMode9";
        version = "unstable";
        src = builtins.path { path = ./.; name = "GodMode9"; };

        nativeBuildInputs = with pkgs; [
          python3Packages.python
          ( python3Packages.callPackage ./firmtool.nix { } )
        ];

        preBuild = pkgs.devkitNix.devkitARM.shellHook;

        installPhase = ''
          mkdir $out
          cp output/* $out
        '';
      };
      default = godmode9;
    };
  };
}
