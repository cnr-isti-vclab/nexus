cmake_minimum_required(VERSION 3.10)
project(nxsbuild)

set(SOURCES
	main.cpp
	../core/mapped_file.cpp
	../core/mesh.cpp
    ../loaders/meshloader.cpp
    ../loaders/objloader.cpp
    ../loaders/objexporter.cpp
)

add_executable(nxsbuild ${SOURCES})

target_include_directories(nxsbuild PRIVATE ../core)

install(TARGETS nxsbuild DESTINATION bin)
