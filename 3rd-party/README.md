This directory contains the 3rd-party dependencies required to build the plug-in.

### Adobe Photoshop Plug-In and Connection SDK

You will need to download the latest Adobe Photoshop Plug-In and Connection SDK from http://www.adobe.com/devnet/photoshop/sdk.html and unzip it into this folder.
Extract the downloaded SDK and rename the folder to `adobe_photoshop_sdk`.

### AOM

Clone AOM from your preferred tag:

`git clone -b v3.2.0 --depth 1 https://aomedia.googlesource.com/aom`

Change into the `aom` directory and create a build directory.
In this example a x86_64 build using Visual Studio 2019 is used. 

`cd aom`   
`mkdir build-x64 && cd build-x64`   
`cmake -G "Visual Studio 16 2019" -A x64 -DCMAKE_BUILD_TYPE=Release -DENABLE_DOCS=0 -DENABLE_EXAMPLES=0 -DENABLE_TESTDATA=0 -DENABLE_TESTS=0 -DENABLE_TOOLS=0 ..`   
`cmake --build .`

The generated AOM library should be located in `aom/build-x64/Release`, this library will be used when building `libheif`.

### libheif

Clone libheif from your preferred tag:

`git clone -b v1.12.0 --depth 1 https://github.com/strukturag/libheif`

Change into the `libheif` directory and create a build directory.
In this example a x86_64 build using Visual Studio 2019 is used. 

`cd libheif`   
`mkdir build-x64 && cd build-x64`   
`cmake -G "Visual Studio 16 2019" -A x64 -DBUILD_SHARED_LIBS=OFF -DWITH_EXAMPLES=OFF -DAOM_INCLUDE_DIR=..\..\aom -DAOM_LIBRARY=..\..\aom\build-x64\Release\aom.lib ..`   
`cmake --build .`

You will need to add `LIBHEIF_STATIC_BUILD` to the preprocessor settings page in the libheif project properties,
and remove the `HAS_VISIBILITY` definition if present.

The generated libheif library should be located in `libheif/build-x64/Release`.