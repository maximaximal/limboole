Build using CMake
=================

The CMake build process handles automatic downloads of
all dependencies and also supports direct cross compilation
and packaging of generated binaries.

    mkdir build
    cd build
    cmake ..
    make

To create linux packages in build/packages, run

    make package

CMake can also be used to enable/disable picosat or lingeling
usage by specifying -DUSE_PICOSAT=On/Off and -DUSE_LINGELING=On/Off.

Cross Compilation
-----------------

Automatic cross compilation can be archieved using
various methods. The easiest is using the dockcross compiler
toolchains based on docker: https://github.com/dockcross/dockcross

An example of using that toolchain is provided below. The current lingeling
release does not work on windows, so only picosat is used.

    docker run --rm dockcross/windows-x64 > ./dockcross-windows-x64
    chmod +x dockcross-windows-x64
    ./dockcross-windows-x64 cmake -Bbuild-windows-x64 -H. -DCMAKE_BUILD_TYPE=Release -DUSE_LINGELING=Off
    ./dockcross-windows-x64 make -C build-windows-x64

To build packages, run

    ./dockcross-windows-x64 make -C build-windows-x64 package

A generic script is provided to automatically create packages for Linux and Windows,
it can be executed like this:

    ./create_packages.sh

Max Heisinger
Wed Oct 17 14:00:00 CEST 2018

Build using Autotools
=====================

After extracting and compiling at least one of
Lingeling or PicoSAT in '../lingeling'
respectively '../picosat' issue:

   ./configure && make

This also builds 'testlimboole' and 'dimacs2boole'.

You can also compile with debugging support, or force
inclusion of only one of PicoSAT or Lingeling,
e.g., see './configure.sh -h'.

Armin Biere
Mon Oct  7 16:34:15 CEST 2013
