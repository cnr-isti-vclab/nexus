#export CMAKE_PREFIX_PATH=path to Qt/5.5/gcc_64/ (save for bin/qmake)

cmake_minimum_required(VERSION 3.10)
project(nxsedit)

SET(HEADERS
	${VCGDIR}/wrap/system/qgetopt.h
	../src/meco/zpoint.h
	../src/meco/model.h
	../src/meco/range.h
	../src/meco/fpu_precision.h
	../src/meco/math_class.h
	../src/meco/cstream.h
	extractor.h
)

SET(SOURCES
	${VCGDIR}/wrap/system/qgetopt.cpp
	main.cpp
	extractor.cpp
)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

add_executable(nxsedit ${SOURCES} ${HEADERS})

target_link_libraries(nxsedit PUBLIC nexus)

# --------------------------------------------------------

project(nxscompress)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

SET(HEADERS
	${VCGDIR}/wrap/system/qgetopt.h
	extractor.h
	../src/meco/zpoint.h
	../src/meco/model.h
	../src/meco/range.h
	../src/meco/fpu_precision.h
	../src/meco/bytestream.h
	../src/meco/math_class.h
	../src/meco/cstream.h
)

SET(SOURCES
	${VCGDIR}/wrap/system/qgetopt.cpp
	main_compress.cpp
	extractor.cpp
)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

add_executable(nxscompress ${SOURCES} ${HEADERS})

target_link_libraries(nxscompress PUBLIC nexus)

install(TARGETS nxscompress nxsedit)
