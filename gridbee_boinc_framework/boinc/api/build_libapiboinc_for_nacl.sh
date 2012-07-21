#!/bin/bash -e

SDK="/home/kalmi/ik/nacl_sdk/pepper_15"

function create_allIn_a {
  TMPDIR="`mktemp -d`"
  echo $TMPDIR
  cp $O_FILES $TMPDIR
  pushd $TMPDIR &> /dev/null
  $LD -Ur * -o BOINC.o
  $AR rvs $OUTPUT_NAME BOINC.o
  $RANLIB $OUTPUT_NAME
  popd &> /dev/null
}

function compile {
  $CXX -O2 -DUSE_SSE -DUSE_SSE2 -DUSE_SSE3 -D__SSE__ -D__SSE2__ -D__SSE3__ -msse3 -c -Wfatal-errors -I. -I.. \
  base64.cpp \
	cert_sig.cpp coproc.cpp \
	hostinfo.cpp md5.c \
	md5_file.cpp mem_usage.cpp mfile.cpp miofile.cpp msg_log.cpp \
	notice.cpp parse.cpp prefs.cpp \
	procinfo_unix.cpp str_util.cpp \
	util.cpp # 2>/dev/null 1>/dev/null
}



CXX="$SDK/toolchain/linux_x86_newlib/bin/i686-nacl-g++"
compile

BIN_BASE="$SDK/toolchain/linux_x86_newlib/bin"
LD="$BIN_BASE/i686-nacl-ld"
AR="$BIN_BASE/i686-nacl-ar"
RANLIB="$BIN_BASE/i686-nacl-ranlib"
O_FILES="`find $(pwd) | grep "\.o$"`"
LIBDIR="/home/kalmi/ik/NaClWorker/bin/lib/lib32"
mkdir -p $LIBDIR
if [ -f $LIBDIR/libboinc.a ]; then
  rm $LIBDIR/libboinc.a
fi
OUTPUT_NAME="`readlink -m $LIBDIR/libboinc.a`"
create_allIn_a

echo 32bitDone


CXX=$SDK/toolchain/linux_x86_newlib/bin/x86_64-nacl-g++
compile

BIN_BASE="$SDK/toolchain/linux_x86_newlib/bin"
LD="$BIN_BASE/x86_64-nacl-ld"
AR="$BIN_BASE/x86_64-nacl-ar"
RANLIB="$BIN_BASE/x86_64-nacl-ranlib"
O_FILES="`find $(pwd) | grep "\.o$"`"
LIBDIR="/home/kalmi/ik/NaClWorker/bin/lib/lib64"
mkdir -p $LIBDIR
if [ -f $LIBDIR/libboinc.a ]; then
  rm $LIBDIR/libboinc.a
fi
OUTPUT_NAME="`readlink -m $LIBDIR/libboinc.a`"
create_allIn_a


echo 64bitDone
