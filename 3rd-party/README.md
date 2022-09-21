This directory contains the 3rd-party dependencies required to build the plug-in.

### Adobe Photoshop Plug-In and Connection SDK

You will need to download the latest Adobe Photoshop Plug-In and Connection SDK from http://www.adobe.com/devnet/photoshop/sdk.html and unzip it into this folder.
Extract the downloaded SDK and rename the folder to `adobe_photoshop_sdk`.

### AOM

Clone AOM from your preferred tag:

`git clone -b v3.5.0 --depth 1 https://aomedia.googlesource.com/aom`

Change into the `aom` directory and create a build directory.

`cd aom`   

The build directory name and CMake build commands will depend on the target platform.

Windows x86 (32-bit):

`mkdir build-x86 && cd build-x86`   
`cmake -G "Visual Studio 16 2019" -A Win32 -DCMAKE_BUILD_TYPE=Release -DENABLE_DOCS=0 -DENABLE_EXAMPLES=0 -DENABLE_TESTDATA=0 -DENABLE_TESTS=0 -DENABLE_TOOLS=0 ..`   
`cmake --build .`

Windows x64:

`mkdir build-x64 && cd build-x64`   
`cmake -G "Visual Studio 16 2019" -A x64 -DCMAKE_BUILD_TYPE=Release -DENABLE_DOCS=0 -DENABLE_EXAMPLES=0 -DENABLE_TESTDATA=0 -DENABLE_TESTS=0 -DENABLE_TOOLS=0 ..`   
`cmake --build .`

Windows ARM64:

`mkdir build-arm64 && cd build-arm64`   
`cmake -G "Visual Studio 16 2019" -A ARM64 -DCMAKE_SYSTEM_PROCESSOR=arm64 -DCONFIG_RUNTIME_CPU_DETECT=0 -DAOM_TARGET_CPU=generic -DCMAKE_BUILD_TYPE=Release -DENABLE_DOCS=0 -DENABLE_EXAMPLES=0 -DENABLE_TESTDATA=0 -DENABLE_TESTS=0 -DENABLE_TOOLS=0 ..`   
`cmake --build .`

The generated AOM library should be located in `aom/build-<platform>/Release`, this library will be used when building `libheif`.

### libheif

Clone libheif from your preferred tag:

`git clone -b v1.13.0 --depth 1 https://github.com/strukturag/libheif`

Change into the `libheif` directory and create a build directory.

`cd libheif`   

The build directory name and CMake build commands will depend on the target platform.

Windows x86 (32-bit):

`mkdir build-x86 && cd build-x86`   
`cmake -G "Visual Studio 16 2019" -A Win32 -DBUILD_SHARED_LIBS=OFF -DWITH_EXAMPLES=OFF -DAOM_INCLUDE_DIR=..\..\aom -DAOM_LIBRARY=..\..\aom\build-x86\Release\aom.lib ..`   
`cmake --build .`

Windows x64:

`mkdir build-x64 && cd build-x64`   
`cmake -G "Visual Studio 16 2019" -A x64 -DBUILD_SHARED_LIBS=OFF -DWITH_EXAMPLES=OFF -DAOM_INCLUDE_DIR=..\..\aom -DAOM_LIBRARY=..\..\aom\build-x64\Release\aom.lib ..`   
`cmake --build .`

Windows ARM64:

`mkdir build-arm64 && cd build-arm64`   
`cmake -G "Visual Studio 16 2019" -A ARM64 -DBUILD_SHARED_LIBS=OFF -DWITH_EXAMPLES=OFF -DAOM_INCLUDE_DIR=..\..\aom -DAOM_LIBRARY=..\..\aom\build-arm64\Release\aom.lib ..`   
`cmake --build .

You will need to add `LIBHEIF_STATIC_BUILD` to the preprocessor settings page in the libheif project properties,
and remove the `HAS_VISIBILITY` definition if present.

The generated libheif library should be located in `libheif/build-<platform>/Release`.