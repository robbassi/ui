{pkgs ? import <nixpkgs> {} }:
let
  crossCompile = hostPkgs:
    hostPkgs.callPackage (crossCompileFor hostPkgs) {};
  crossCompileFor = hostPkgs: {lib}: let
    inherit (hostPkgs.targetPlatform.uname) system;
    inherit (hostPkgs.stdenv) isDarwin isLinux;
    isWindows = hostPkgs.stdenv.targetPlatform.isWindows;
    isLinuxOrDarwin = isLinux || isDarwin;
    arch = hostPkgs.targetPlatform.system;
    SDL2_pkg = args: (hostPkgs.SDL2.override args).overrideAttrs (prev: rec {
      meta.platforms = lib.platforms.all;
    });
    SDL2 = hostPkgs.callPackage SDL2_pkg {withStatic = true;};
    SDL2_ttf = hostPkgs.SDL2_ttf.overrideAttrs (prev: rec {
      meta.platforms = lib.platforms.all;
      configureFlags = [
        "--without-shared"
        "--disable-shared"
        "--enable-static"
      ];
      buildInputs = [ SDL2 ];
      nativeBuildInputs = [ SDL2 hostPkgs.pkg-config ];
      outputs = ["out" "dev"];
    });
    SDL2_image = hostPkgs.SDL2_image.overrideAttrs (prev: rec {
      meta.platforms = lib.platforms.all;
      # We disable most of the image formats to make cross compat easier.
      configureFlags =
        prev.configureFlags
        ++ [
          "--without-shared"
          "--disable-shared"
          "--enable-static"
          "--disable-gif"
          "--disable-tif"
          "--disable-xpm"
          "--disable-webp"
        ];
      buildInputs =
        lib.optional isDarwin hostPkgs.libjpeg
        ++ [
          SDL2
          hostPkgs.libpng
          hostPkgs.zlib
        ];
      nativeBuildInputs = [ SDL2 hostPkgs.pkg-config ];
      outputs = ["out" "dev"];
    });
    SDL2_mixer = hostPkgs.SDL2_mixer.overrideAttrs (prev: rec {
      meta.platforms = lib.platforms.all;
      configureFlags =
        lib.optionals isLinuxOrDarwin prev.configureFlags
        ++ lib.optionals (!isDarwin) [
          "--without-shared"
          "--disable-shared"
          "--enable-static"
        ]
        ++ [
          "--disable-music-flac"
          "--disable-music-ogg"
          "--disable-music-mp3-mpg123"
          "--disable-music-opus"
        ];
      propagatedBuildInputs =
        [SDL2]
        ++ lib.optional isLinux hostPkgs.fluidsynth;
      nativeBuildInputs = [ SDL2 hostPkgs.pkg-config ];
      outputs = ["out" "dev"];
    });
    linkerFlags = lib.concatStringsSep " " ([
        "-lm"
      ]
      ++ lib.optionals isDarwin [
        "-lpng"
        "-ljpeg"
      ]
      ++ lib.optionals isLinux [
        "-rdynamic"
        "-lrt"
        "-lpthread"
        "-lX11"
        "-lXext"
        "-lXss"
        "-lXrandr"
        "-lXi"
        "-lXcursor"
        "-lXfixes"
        "-lGL"
        "-lfluidsynth"
        "-lpng"
      ]
      ++ lib.optionals isWindows [
        "-static-libgcc"
        "-static-libstdc++"
        "-mwindows"
        # Increase the default stack size on Windows, this helps avoid stack
        # overflows caused by large stack allocations - eg entity data.
         "-Wl,--stack,41943040"
        "-Wl,--dynamicbase"
        "-Wl,--nxcompat"
        "-Wl,--high-entropy-va"
        "-lmingw32"
        "-ldinput8"
        "-ldxguid"
        "-ldxerr8"
        "-luser32"
        "-lgdi32"
        "-lwinmm"
        "-limm32"
        "-lole32"
        "-loleaut32"
        "-lshell32"
        "-lsetupapi"
        "-lversion"
        "-luuid"
        "-lrpcrt4"
      ]);
    gccFlags = lib.concatStringsSep " " [
      "-g"
      "-O0"
      "-ggdb"
      # "-O2"
      "-Wall"
      "-pedantic-errors"
    ];
    dylibOrArchive =
      lib.optionalString isDarwin "dylib"
      + lib.optionalString (!isDarwin) "a";
    buildScript = pkgs.writeShellScriptBin "mk" ''
      commit_hash=$(${pkgs.git}/bin/git rev-parse --short HEAD)
      cd src
      OUT="${arch}" \
      CXX_FLAGS="-std=c++20 -D COMMIT_HASH=\\\"$commit_hash\\\" \
                 ${gccFlags}" \
      INC="-I${SDL2.dev}/include/SDL2 \
           -I${SDL2.dev}/include/SDL2 \
           -I${SDL2_image.dev}/include/SDL2 \
           -I${SDL2_ttf.dev}/include/SDL2 \
           -I${SDL2_mixer.dev}/include/SDL2 \
      LIB="${lib.optionalString (!isDarwin) "${SDL2}/lib/libSDL2main.a"} \
           ${SDL2}/lib/libSDL2.${dylibOrArchive} \
           ${SDL2_image}/lib/libSDL2_image.a \
           ${SDL2_ttf}/lib/libSDL2_ttf.a \
           ${SDL2_mixer}/lib/libSDL2_mixer.${dylibOrArchive} \
           ${linkerFlags}" \
      make ${arch}
      cp ${arch}* ../
    '';
    testBuildScript = pkgs.writeShellScriptBin "mk-test" ''
      commit_hash=$(${pkgs.git}/bin/git rev-parse --short HEAD)
      cd test
      OUT="${arch}-tests" \
      CXX_FLAGS="-std=c++20 -D COMMIT_HASH=\\\"$commit_hash\\\" \
                 ${gccFlags}" \
      INC="-I${SDL2.dev}/include/SDL2 \
           -I${SDL2.dev}/include/SDL2 \
           -I${SDL2_image.dev}/include/SDL2 \
           -I${SDL2_ttf.dev}/include/SDL2 \
           -I${SDL2_mixer.dev}/include/SDL2 \
           -I${hostPkgs.catch2_3}/include" \
      LIB="${SDL2}/lib/libSDL2.${dylibOrArchive} \
           ${SDL2_image}/lib/libSDL2_image.a \
           ${SDL2_ttf}/lib/libSDL2_ttf.a \
           ${SDL2_mixer}/lib/libSDL2_mixer.${dylibOrArchive} \
           ${hostPkgs.catch2_3}/lib/*.a \
           ${linkerFlags}" \
      make ${arch}-tests
      cp ${arch}* ../
    '';
  in
    pkgs.symlinkJoin {
        name = "${arch}-deps";
        paths = [
          SDL2
          SDL2.dev
          SDL2_image
          SDL2_image.dev
          SDL2_mixer
          SDL2_mixer.dev
          SDL2_ttf
          SDL2_ttf.dev
        ];
      };
  native = crossCompile pkgs;
  windows = crossCompile pkgs.pkgsCross.mingwW64;
in
  pkgs.runCommand "deps" {} ''
    mkdir -p $out/{native,windows}
    cp -r ${native}/* $out/linux/
    cp -r ${windows}/* $out/windows/
  ''
