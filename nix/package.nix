{
    lib,
    stdenv,
    cmake,
    python3,
    src,
}:

stdenv.mkDerivation {
    pname = "moofetch";
    version = "0.1.0";

    inherit src;

    nativeBuildInputs = [
        cmake
        python3
    ];

    # The test suite drives a pseudo-terminal, which does not work in the sandbox.
    # Run `python3 tests/pty_smoke.py build/moofetch` manually instead.
    doCheck = false;

    meta = {
        description = "System information tool with animated ASCII logos (fastfetch fork)";
        homepage = "https://github.com/lilithmoder/moofetch";
        license = lib.licenses.mit;
        mainProgram = "moofetch";
        platforms = lib.platforms.unix;
    };
}
