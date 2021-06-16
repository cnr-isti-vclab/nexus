cmake_minimum_required(VERSION 3.1)
project(nexus)

set(HEADERS
	common/signature.h
	common/cone.h
	common/virtualarray.h
	common/nexusfile.h
	common/qtnexusfile.h
)

set(SOURCES
	common/virtualarray.cpp
	common/cone.cpp
	common/qtnexusfile.cpp
)

add_library(nexus SHARED
	${SOURCES} ${HEADERS})

target_include_directories(nexus PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/common)

target_link_libraries(nexus PUBLIC vcglib corto Qt5::Widgets)
