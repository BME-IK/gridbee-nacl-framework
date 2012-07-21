@echo on
setlocal
@echo on
call scons.bat

REM ========================================================
REM > TODO: Specify the correct values for these variables
REM > depending on where you installed the Native Client SDK
REM > and what pepper version you would like to use. This
REM > script has been tested with pepper_16 and is not
REM > guaranteed to work with other pepper versions without
REM > further modification.

set NACL_SDK_ROOT=D:\nacl_sdk
set NACL_TARGET_PLATFORM=pepper_16
set LIBDIR32=lib\lib32
set LIBDIR64=lib\lib64

REM >
REM ========================================================




REM 　　　　　　　　　　　　　　　　　　　　　　　　　　　　

set NACL_PLATFORM_DIR=%NACL_SDK_ROOT%\%NACL_TARGET_PLATFORM%

set TMPDIR=temp
md %TMPDIR%
echo %TMPDIR%
set BASE=%CD%

REM ================== x86 ============
:x86
set SDK=%NACL_PLATFORM_DIR%
set BIN_BASE=%SDK%\toolchain\win_x86\bin
set LD=%BIN_BASE%\i686-nacl-ld
set AR=%BIN_BASE%\i686-nacl-ar
set RANLIB=%BIN_BASE%\i686-nacl-ranlib
set LIBPPAPI_CPP_A=%SDK%\toolchain\win_x86_newlib\x86_64-nacl\lib32\libppapi_cpp.a
del /q %TMPDIR%\*.*
pushd opt_x86_32
call :fileList
popd

set LIBDIR=%LIBDIR32%
md %LIBDIR%
set OUTPUT_NAME=%BASE%\%LIBDIR%\libgridbee.a
IF EXIST %LIBDIR%\libgridbee.a (
	del /s %LIBDIR%\libgridbee.a
)
call :create_allIn_a

REM ================== x64 ============
:x64
set SDK=%NACL_PLATFORM_DIR%
set BIN_BASE=%SDK%\toolchain\win_x86\bin
set LD=%BIN_BASE%\x86_64-nacl-ld
set AR=%BIN_BASE%\x86_64-nacl-ar
set RANLIB=%BIN_BASE%\x86_64-nacl-ranlib
set LIBPPAPI_CPP_A=%SDK%\toolchain\win_x86_newlib\x86_64-nacl\lib\libppapi_cpp.a
del /q %TMPDIR%\*.*
pushd opt_x86_64
call :fileList
popd

set LIBDIR=%LIBDIR64%
md %LIBDIR%
set OUTPUT_NAME=%BASE%\%LIBDIR%\libgridbee.a
IF EXIST %LIBDIR%\libgridbee.a (
	del /s %LIBDIR%\libgridbee.a
)
call :create_allIn_a

rd /s /q %TMPDIR%
scons.bat -c

goto end

REM ------------------
:create_allIn_a
pushd %TMPDIR%
%AR% x %LIBPPAPI_CPP_A%
%LD% -Ur * -o FRAMEWORK.o
%AR% rvs %OUTPUT_NAME% FRAMEWORK.o
%RANLIB% %OUTPUT_NAME%
popd
goto end
REM ------------------

REM -------------------
:fileList
set O_FILES=
FOR /R %%f IN (*.o) DO (
	call :x %%f
)
del %TMPDIR%\my_main.o

goto end
REM ------------------

REM ------------------
:x
echo COPY %1 %TMPDIR%
copy %1 %TMPDIR%
set O_FILES=%1 %O_FILES% 
REM -------------------

:end