#export CMAKE_PREFIX_PATH=path to Qt/5.5/gcc_64/ (save for bin/qmake)

cmake_minimum_required(VERSION 3.1)
project(nxsbuild)

SET(HEADERS
	${VCGDIR}/wrap/system/qgetopt.h
	${VCGDIR}/wrap/ply/plylib.h
	partition.h
	kdtree.h
	trianglesoup.h
	mesh.h
	nexusbuilder.h
	tmesh.h
	vertex_cache_optimizer.h
	texpyramid.h
)

SET(SOURCES
	${VCGDIR}/wrap/system/qgetopt.cpp
	${VCGDIR}/wrap/ply/plylib.cpp
	main.cpp
	kdtree.cpp
	mesh.cpp
	nexusbuilder.cpp
	tmesh.cpp
	texpyramid.cpp
)

cmake_minimum_required(VERSION 3.1)

project(nxsbuild)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

add_executable(nxsbuild ${SOURCES} ${HEADERS})

target_link_libraries(nxsbuild PUBLIC nexus)
