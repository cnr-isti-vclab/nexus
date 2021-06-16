#export CMAKE_PREFIX_PATH=path to Qt/5.5/gcc_64/ (save for bin/qmake)

cmake_minimum_required(VERSION 3.1)
project(nxsedit)

set (CMAKE_CXX_STANDARD 11)

SET(HEADERS
	${VCGDIR}/wrap/system/qgetopt.h
	../common/nexusdata.h
	../common/traversal.h
	../nxszip/zpoint.h
	../nxszip/model.h
	../nxszip/range.h
	../nxszip/fpu_precision.h
	../nxszip/bytestream.h
	../nxszip/math_class.h
	../nxszip/bitstream.h
	../nxszip/tunstall.h
	../nxszip/cstream.h
	../nxszip/meshcoder.h
	../nxszip/meshdecoder.h
	extractor.h
)

SET(SOURCES
	${VCGDIR}/wrap/system/qgetopt.cpp
	${VCGDIR}/wrap/ply/plylib.cpp
	../common/nexusdata.cpp
	../common/traversal.cpp
	../nxszip/abitstream.cpp
	../nxszip/atunstall.cpp
	../nxszip/meshcoder.cpp
	../nxszip/meshdecoder.cpp
	../common/qtnexusfile.cpp
	main.cpp
	extractor.cpp
)

cmake_minimum_required(VERSION 3.1)

project(nxsedit)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt5Widgets)

add_executable(nxsedit ${SOURCES} ${HEADERS})

target_link_libraries(nxsedit PUBLIC nexus)

# --------------------------------------------------------

project(nxscompress)

set (CMAKE_CXX_STANDARD 11)

SET(HEADERS
	${VCGDIR}/wrap/system/qgetopt.h
	../common/nexusdata.h
	../common/traversal.h
	../nxszip/zpoint.h
	../nxszip/model.h
	../nxszip/range.h
	../nxszip/fpu_precision.h
	../nxszip/bytestream.h
	../nxszip/math_class.h
	../nxszip/bitstream.h
	../nxszip/tunstall.h
	../nxszip/cstream.h
	../nxszip/meshcoder.h
	../nxszip/meshdecoder.h
	extractor.h

)

SET(SOURCES
	${VCGDIR}/wrap/system/qgetopt.cpp
	${VCGDIR}/wrap/ply/plylib.cpp
	../common/nexusdata.cpp
	../common/traversal.cpp
	../nxszip/meshcoder.cpp
	../nxszip/meshdecoder.cpp
	../nxszip/abitstream.cpp
	../nxszip/atunstall.cpp
	main_compress.cpp
	extractor.cpp
)

cmake_minimum_required(VERSION 3.1)

project(nxscompress)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt5Widgets)

add_executable(nxscompress ${SOURCES} ${HEADERS})

target_link_libraries(nxscompress PUBLIC nexus)
