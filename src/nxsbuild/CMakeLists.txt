#export CMAKE_PREFIX_PATH=path to Qt/5.5/gcc_64/ (save for bin/qmake)

cmake_minimum_required(VERSION 3.1)
project(nxsbuild)

SET(HEADERS
	${VCGDIR}/wrap/system/qgetopt.h
	partition.h
	trianglesoup.h
	vertex_cache_optimizer.h
)

SET(SOURCES
	${VCGDIR}/wrap/system/qgetopt.cpp
	main.cpp
)

cmake_minimum_required(VERSION 3.1)

project(nxsbuild)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

add_executable(nxsbuild ${SOURCES} ${HEADERS})

target_link_libraries(nxsbuild PUBLIC nexus)

install(TARGETS nxsbuild)
