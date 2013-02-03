About
=====
The solc utility is a command-line utility for converting
sol source files to libsol-compatible C source.

Building solc
=============
The process to build solc is the same as that of libsol; see
the libsol README for the build process information.

Using libsol
============
Once built and installed, solc can be called from the command-line.
To use it, simply type `solc <filename>`. It will generate a
.c file with the same name as the original .sol input.

To compile and run the resulting file, you'll need to have already
built libsol. You can then compile the file with a standard C
compiler, preferably either GCC or Clang. Here is a sample command,
using Clang:

    clang -lsol -o my-program my-program.c

You can then run the program by typing:

    ./my-program
