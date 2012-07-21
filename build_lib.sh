#!/bin/bash -e

# ========================================================
# > TODO: Specify the correct values for these variables
# > depending on where you installed the Native Client SDK
# > and what pepper version you would like to use. This
# > script has been tested with pepper_16 and is not
# > guaranteed to work with other pepper versions without
# > further modification.

SDK="$HOME/nacl_sdk/pepper_16"
LIBDIR32="lib/lib32"
LIBDIR64="lib/lib64"

# >
# ========================================================




function create_allIn_a {
TMPDIR="`mktemp -d`"
  echo $TMPDIR
  cp $O_FILES $TMPDIR
  pushd $TMPDIR &> /dev/null
  $AR x $LIBPPAPI_CPP_A
  $LD -Ur * -o FRAMEWORK.o
  $AR rvs $OUTPUT_NAME FRAMEWORK.o
  $RANLIB $OUTPUT_NAME
  popd &> /dev/null
}

./scons


BIN_BASE="$SDK/toolchain/linux_x86/bin"
LD="$BIN_BASE/i686-nacl-ld"
AR="$BIN_BASE/i686-nacl-ar"
RANLIB="$BIN_BASE/i686-nacl-ranlib"
LIBPPAPI_CPP_A="$SDK/toolchain/linux_x86_newlib/x86_64-nacl/lib32/libppapi_cpp.a"
O_FILES="`find $(pwd)/opt_x86_32 | grep .o$ | grep --invert-match my_main.o | tr "\n" " "`"
LIBDIR=$LIBDIR32
mkdir -p $LIBDIR
if [ -f $LIBDIR/libgridbee.a ]; then
  rm $LIBDIR/libgridbee.a
fi
OUTPUT_NAME="`readlink -m $LIBDIR/libgridbee.a`"
create_allIn_a


BIN_BASE="$SDK/toolchain/linux_x86/bin"
LD="$BIN_BASE/x86_64-nacl-ld"
AR="$BIN_BASE/x86_64-nacl-ar"
RANLIB="$BIN_BASE/x86_64-nacl-ranlib"
LIBPPAPI_CPP_A="$SDK/toolchain/linux_x86_newlib/x86_64-nacl/lib64/libppapi_cpp.a"
O_FILES="`find $(pwd)/opt_x86_64 | grep .o$ | grep --invert-match my_main.o | tr "\n" " "`"
LIBDIR=$LIBDIR64
mkdir -p $LIBDIR
if [ -f $LIBDIR/libgridbee.a ]; then
  rm $LIBDIR/libgridbee.a
fi
OUTPUT_NAME="`readlink -m $LIBDIR/libgridbee.a`"
create_allIn_a

 ./scons -c
