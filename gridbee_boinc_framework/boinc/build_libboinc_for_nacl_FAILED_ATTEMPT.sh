#!/bin/bash -e

SDK="/home/kalmi/ik/nacl_sdk/pepper_15"

function compile { 
  PATH=$MYPATH CXX=$CXX ./configure --disable-client --disable-server --disable-manager --disable-fcgi 
  cd lib
  make clean
  PATH=$MYPATH make libboinc.la
  cp .libs/libboinc.a $LIBDIR
  cd ..
}

#LIBDIR="/home/kalmi/ik/NaClWorker/bin/lib/lib32"
#CXX="$SDK/toolchain/linux_x86/bin/i686-nacl-g++"
#compile

LIBDIR="/home/kalmi/ik/NaClWorker/bin/lib/lib64"
MYPATH="$SDK/toolchain/linux_x86_newlib/x86_64-nacl/bin:$SDK/toolchain/linux_x86_newlib/libexec/gcc/x86_64-nacl/4.4.3/:$SDK/toolchain/linux_x86_newlib/x86_64-nacl/include/:$PATH"
EXTRA="--host="
CXX="$SDK/toolchain/linux_x86/bin/x86_64-nacl-g++"
compile



