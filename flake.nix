{
  description = "TNFS SE (1996) port: reverse engineering + portable C99/SDL3 reimplementation";

  inputs = {
    # 26.05 is the last nixpkgs release that builds for x86_64-darwin (Intel Mac).
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-26.05-darwin";
  };

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-darwin" "aarch64-darwin" "x86_64-linux" "aarch64-linux" ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f (import nixpkgs {
        inherit system;
        config.allowDeprecatedx86_64Darwin = true;
      }));

      # Prebuilt Watcom 11.0c 32-bit runtime libraries (OMF), republished by the
      # Open Watcom project. Closest freely available match to the Watcom 10.x
      # runtime linked into NFS_WIN.EXE; used only for signature matching.
      watcom11Zips = {
        clib_w32 = "sha256-kKfy+tTw5sI1RlaX4niuBXEh6C9+moUuS3MIlRrv/z8=";
        clib_a32 = "sha256-v96jg/xrobPg6GkexNPyHExNJ9eYxyPhSOlqg/UjYus=";
        cpplib_w32 = "sha256-WNCrBy2u8YcnXs/AvUFOPhJooxhOwxUx+zbLH5OkxTY=";
        cpplib_a32 = "sha256-Nm3Er8BnZB5gtWezMo70OYjzNkBxr0HzwAXtQmfICUI=";
      };
      watcom11Libs = pkgs: pkgs.runCommand "watcom11-libs" { nativeBuildInputs = [ pkgs.unzip ]; } ''
        mkdir -p $out
        ${nixpkgs.lib.concatStrings (nixpkgs.lib.mapAttrsToList (name: hash: ''
          unzip -qo ${pkgs.fetchurl {
            url = "https://github.com/open-watcom/open-watcom-1.9/releases/download/w11.0c-zips/${name}.zip";
            inherit hash;
          }} 'lib386/*' -d $out
        '') watcom11Zips)}
      '';
    in
    {
      packages = forAllSystems (pkgs: {
        watcom11-libs = watcom11Libs pkgs;
      });

      devShells = forAllSystems (pkgs: {
        # Native build of the port (macOS / Linux).
        default = pkgs.mkShell {
          packages = with pkgs; [ cmake ninja pkg-config sdl3 lldb ];
        };

        # WebAssembly build of the port.
        web = pkgs.mkShell {
          packages = with pkgs; [ emscripten cmake ninja python3 ];
          shellHook = ''
            export EM_CACHE="$PWD/.cache/emscripten"
          '';
        };

        # Reverse engineering: Ghidra (prebuilt; the source build is broken on x86_64-darwin) headless + scripting,
        # DOSBox-X to run the original as reference, Watcom runtime libraries for signatures,
        # Unicorn to execute original functions as test oracles.
        re = pkgs.mkShell {
          packages = with pkgs; [ ghidra-bin (python3.withPackages (ps: [ ps.unicorn ])) dosbox-x ];
          WATCOM11_LIBS = "${watcom11Libs pkgs}/lib386";
        };
      });
    };
}
