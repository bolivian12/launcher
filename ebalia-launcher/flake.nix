{
  description = "EBALIA Launcher — Lost Versions + full in-process Minecraft client";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
      in {
        packages.default = pkgs.stdenv.mkDerivation {
          pname = "ebalia-launcher";
          version = "1.0.0";

          src = pkgs.lib.cleanSourceWith { src = ./.; filter = path: type: !(builtins.elem (baseNameOf path) [ "patreon.json" ".env" ".env.local" ".env.production" ".neon" "node_modules" "private" "__pycache__" "build" "legacy" "Bexe16.4" "codigo generacion" "result" "result-v4" "artifacts" ]); };

          nativeBuildInputs = [ pkgs.cmake pkgs.qt6.wrapQtAppsHook pkgs.pkg-config ];
          buildInputs = [ pkgs.qt6.qtbase pkgs.qt6.qttools pkgs.qt6.qtimageformats pkgs.libarchive ];

          cmakeFlags = [
            "-DCMAKE_BUILD_TYPE=Release"
          ];



          doCheck = true;
          # Qt settings and font caches must be writable inside the Nix sandbox.
          checkPhase = ''
            runHook preCheck
            export HOME="$TMPDIR/ebalia-test-home"
            export XDG_CONFIG_HOME="$HOME/.config"
            export XDG_CACHE_HOME="$HOME/.cache"
            export XDG_DATA_HOME="$HOME/.local/share"
            mkdir -p "$XDG_CONFIG_HOME" "$XDG_CACHE_HOME" "$XDG_DATA_HOME"
            export QT_QPA_PLATFORM=offscreen
            ctest --output-on-failure
            runHook postCheck
          '';
          qtWrapperArgs = [ "--set" "EBALIA_JAVA_PATHS" "${pkgs.jdk8}/bin/java:${pkgs.jdk17}/bin/java:${pkgs.jdk21}/bin/java:${pkgs.jdk25}/bin/java"
            "--prefix" "LD_LIBRARY_PATH" ":" "/run/opengl-driver/lib:${pkgs.lib.makeLibraryPath [ pkgs.libglvnd pkgs.alsa-lib pkgs.libpulseaudio pkgs.libx11 pkgs.libxcursor pkgs.libxext pkgs.libxrandr pkgs.libxrender pkgs.libxxf86vm pkgs.stdenv.cc.cc.lib ]}"
          ];

          meta = with pkgs.lib; {
            description = "EBALIA Launcher — lost versions + full client";
            license = licenses.mit;
            platforms = platforms.all;
          };
        };

        devShells.default = pkgs.mkShell {
          nativeBuildInputs = [ pkgs.cmake pkgs.qt6.wrapQtAppsHook pkgs.pkg-config ];
          buildInputs = [ pkgs.qt6.qtbase pkgs.qt6.qttools pkgs.qt6.qtimageformats pkgs.libarchive ];
        };
      });
}
