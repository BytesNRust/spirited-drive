{
  description = "TNFS SE (1996) port: a portable C99/SDL3 reimplementation";

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
    in
    {
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
      });
    };
}
