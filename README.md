# lzopfs

Lzopfs allows to mount gzip, bzip2, lzo, and lzma compressed files for random read-only access. I.e., files can be seeked arbitrarily without a performance penalty.

# About this fork

Currently, this fork just replaced the SConstruct build system with CMake and added a Readme with build instructions.

# Installation

You will need the libraries for all supported formats as well as CMake and a C++11 compiler. On Debian-like systems, the install instructions for that would look like:

    sudo apt-get install cmake make g++ libfuse-dev liblzo2-dev liblzma-dev zlib1g-dev libbz2-dev

You need to get the source code with:

    git clone git://<repo-url> 

Then, the program can be built with the usual CMake instructions:

    cd lzopfs
    mkdir -p build
    cd build
    cmake ..
    make
    
The binary can then be used directly from the build folder like so:

    mkdir test
    bzip2 -k Makefile
    ./lzopfs Makefile.bz2 test
    ls test/
    fusermount -u test

or it can be installed manualy to, e.g., `/usr/lib/bin`:

    sudo cp lzopfs /usr/lib/bin/

# Usage

    lzopfs <file to mount> <existing mount point folder>

# Notes

  - bzip2, lzo, xz seem to work
  - for gzip, I get `Error reading file Makefile.gz, skipping: inflateReset: (null) (-2)`
