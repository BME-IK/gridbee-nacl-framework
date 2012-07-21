About
=====

The GridBee BOINC/NaCl framework is a modified version of the BOINC framework that allows the creation of Native Client workunits that can communicate with the GridBee Web Computing Framework using the usual functions of the BOINC API.

Building the GridBee NaCl library
=================================

1. Install the [Native Client SDK](https://developers.google.com/native-client/)
2. Fill out the necessary parts of the build_lib.sh and scons files on Linux or the build_lib.bat and scons.bat files on Windows. You need to specify the directory where the NaCl SDK has been installed including the specific pepper API version.
3. Run build_lib.sh or build_lib.bat.

The build scripts have been tested with the `pepper_16` API and they are not guaranteed to work with later versions without further modification. Later versions of the pepper API don't include the scons build tools and rely on makefiles instead. You can still use scons of course but you need to separately install it.

All in all we recommend that you use the `pepper_16 API`, since Chrome is backwards compatible.

Using the framework and the NaCl library
========================================

When compiling your project you have to use the `gridbee_boinc_framework` instead of the original BOINC framework files. You also have to link your project against the compiled `gridbee` library.