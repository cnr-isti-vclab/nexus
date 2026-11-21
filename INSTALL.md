
## Dependencies

### vcglib

[VCG library](https://github.com/cnr-isti-vclab/vcglib) needs to be found in the same directory as the `nexus` directory.

```sh
git clone git@github.com:cnr-isti-vclab/vcglib.git
```

### corto

[Corto](https://github.com/cnr-isti-vclab/corto)

```sh
git clone git@github.com:cnr-isti-vclab/corto.git
```

Corto can be installed (in linux) after compilation:

```sh
cmake ./
make
sudo make install
sudo ldconfig
```

or placed in the parent directory of Nexus, in which case you need to update the include directories in these files

* `./nxsbuild/CMakeLists.txt`
* `./nxsedit/CMakeLists.txt`
* `./nxsview/CMakeLists.txt`

### oneTBB

The default CMake build includes `nxsbuild`, which fetches Trueform. Trueform
requires the oneTBB development package and its CMake package configuration.
The builder also needs METIS, JPEG, PNG, and Draco development files. On
Debian or Ubuntu, install them with:

```sh
sudo apt-get install libtbb-dev libmetis-dev libjpeg-dev libpng-dev libdraco-dev
```

If you only need the viewer and do not want to build `nxsbuild`, configure
CMake with `-DBUILD_NXS_BUILD=OFF`.

### Qt viewer dependencies on Linux

The viewer requires Qt 6.7 or newer, including the `GuiPrivate` and
`ShaderTools` components. When using Debian or Ubuntu Qt packages, install the
private headers, shader tools, XKB, and GLU development packages:

```sh
sudo apt-get install qt6-base-private-dev qt6-shadertools-dev \
  libxkbcommon-dev libglu1-mesa-dev
```

Check that the distribution provides Qt 6.7 or newer; older releases may need
Qt installed from the official installer instead.

### glew

See [Glew](https://github.com/nigels-com/glew) for build instructions.

```sh
git clone git@github.com:nigels-com/glew.git
```

or

```sh
sudo apt-get install libglew-dev
```

or

```sh
brew install glew
```

Again if glew is compiled and not installed, you need to add the include directory in CMakeLists.txt for nxsedit, nxsview and nxsbuild:

```
include_directories(../../../vcglib ../../../vcglib/eigenlib  ../../../glew-2.1.0/include )
```
### curl
nxsview support view over http, curl is needed:

```sh
sudo apt  install libcurl4-openssl-dev
```

## On MacOS

### install xcode

Download from App Store and Open Xcode to initialize.
Then install the command line tools

```sh
xcode-select --install
```

### install cmake

```sh
brew install cmake
```

And in `settings > Build,Execution,Deployment > CMake` add CMake options:

```
-D CMAKE_PREFIX_PATH=/your/path/to/Qt/5.11.1/gcc_64/lib/cmake
```

### install pkg-config

```
brew install pkg-config
```

### install qt5 and get path

```sh
brew install qt5
# copy the path
brew info qt5
# run cmake with qt5 path flag or add flag to shell
cmake . -DCMAKE_PREFIX_PATH=/usr/local/Cellar/qt/5.12.3
```

Within `CMakeLists.txt` you might have to add glew to the `include_directories` line:

```
include_directories(../../../vcglib ../../../vcglib/eigenlib ../../../glew-2.1.0/include )
```


## Build using QMAKE

The available .pro files

* `nxsedit/nxsedit.pro`
* `nxsedit/nxscompress.pro`
* `nxsview/nxsview.pro`

can be loaded using QTCreator or using the commandline.

`nxsbuild` is configured with CMake.

## Build using CMake

```sh
cmake -S . -B build
cmake --build build
```

The viewer defaults to OpenGL. Select the Qt RHI backend with `nxsview --renderer qrhi`.

### nxsedit

In nxsedit.pro and nxscompress.pro, line 20, you might have to

```
replace     unix:INCLUDEPATH += /usr/local/lib
with        unix:INCLUDEPATH += /usr/local/lib /usr/local/include
```

```sh
cd nxsedit
qmake nxsedit.pro
make

qmake nxscompress.pro
make
```

### nxsview

In nxsview.pro, line 20, you might have to

```
replace     unix:INCLUDEPATH += /usr/local/lib
with        unix:INCLUDEPATH += /usr/local/lib /usr/local/include /usr/local/Cellar/glew/2.1.0/include
```

```sh
cd nxsview
qmake nxsview.pro
make
```
