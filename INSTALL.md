**Dependencies**

[VCG library](https://github.com/cnr-isti-vclab/vcglib) to be found in the parent directory of Nexus

[Corto](https://github.com/cnr-isti-vclab/corto)

Glew

**CMAKE**

Within CMakeLists.txt you might have to add  glew to the include_directories line:
```
include_directories(../../../vcglib ../../../vcglib/eigenlib ../../../glew-2.1.0/include )
```

And in settings>Build,Execution,Deployment>CMake add CMake options:
```
-DCMAKE_PREFIX_PATH=/your/path/to/Qt/5.11.1/gcc_64/lib/cmake
```
In linux you can install Corto after download (in Linux).

```
cmake ./
make
sudo make install
sudo ld config
```

**QMAKE**

The .pro files src/nxsbuild/nxsbuild.pro, src/nxsedit/nxsedit.pro and src/nxsview/nxsview.pro can be loaded
using QTCreator or using commandline 

```
cd src/nxsbuild
qmake nxsbuild.pro
make
```

Again you might have to fix the libraries path in the .pro.


