**Dependencies**

[VCG library](https://github.com/cnr-isti-vclab/vcglib) to be found in the parent directory of Nexus

$ git clone git@github.com:cnr-isti-vclab/vcglib.git

[Corto](https://github.com/cnr-isti-vclab/corto)

$ git clone git@github.com:cnr-isti-vclab/corto.git

Add  ../../../corto to the include_directories line:

in ./src/nxsbuild/CMakeLists.txt
in ./src/nxsedit/CMakeLists.txt
in ./src/nxsview/CMakeLists.txt

In linux you can install Corto after download (in Linux).

```
cmake ./
make
sudo make install
sudo ldconfig
```

[Glew](https://github.com/nigels-com/glew)

http://glew.sourceforge.net/

$ git clone ggit@github.com:nigels-com/glew.git

$ brew install glew

On **MacOS**
### install xcode

  Download from App Store and Open Xcode to initialize.
  Then install the command line tools

  $ xcode-select --install

### install cmake

  $ brew install cmake

  And in settings>Build,Execution,Deployment>CMake add CMake options:

  ```
  -DCMAKE_PREFIX_PATH=/your/path/to/Qt/5.11.1/gcc_64/lib/cmake
  ```

### install pkg-config

  $ brew install pkg-config

### install qt5 and get path

  $ brew install qt5
  copy the path
  $ brew info qt5
  run cmake with qt5 path flag or add flag to shell
  $ cmake . -DCMAKE_PREFIX_PATH=/usr/local/Cellar/qt/5.12.3

Within CMakeLists.txt you might have to add  glew to the include_directories line:
```
include_directories(../../../vcglib ../../../vcglib/eigenlib ../../../glew-2.1.0/include )
```


**QMAKE**

The .pro files src/nxsbuild/nxsbuild.pro, src/nxsedit/nxsedit.pro and src/nxsview/nxsview.pro can be loaded using QTCreator or using the commandline.

## nxsbuild
```
cd src/nxsbuild
qmake nxsbuild.pro
make
```

## nxsedit
In nxsedit.pro, line 20,
replace     unix:INCLUDEPATH += /usr/local/lib
with        unix:INCLUDEPATH += /usr/local/lib /usr/local/include

```
cd src/nxsedit
qmake nxsedit.pro
make
```

## nxsview
In nxsview.pro, line 20,
replace     unix:INCLUDEPATH += /usr/local/lib
with        unix:INCLUDEPATH += /usr/local/lib /usr/local/include /usr/local/Cellar/glew/2.1.0/include

```
cd src/nxsview
qmake nxsview.pro
make
```

## nxszip
```
cd src/nxszip
qmake nxszip.pro
make
```

Again you might have to fix the libraries path in the .pro.
