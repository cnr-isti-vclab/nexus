cmake_minimum_required(VERSION 3.10)
project(nxsbuild)

set(SOURCES
	main.cpp
	../core/mapped_file.cpp
	../core/mesh.cpp
	../core/spatial_sort.cpp
	../core/adjacency.cpp
	../core/clustering.cpp
	../core/clustering_metis.cpp
	../core/micro_clustering.cpp
	../core/micro_clustering_metis.cpp
    ../loaders/meshloader.cpp
    ../loaders/objloader.cpp
    ../loaders/objexporter.cpp
    ../loaders/plyexporter.cpp
)

add_executable(nxsbuild ${SOURCES})

# Add METIS library paths
target_include_directories(nxsbuild PRIVATE 
    ../core
    /home/ponchio/local/include
)

target_link_directories(nxsbuild PRIVATE /home/ponchio/local/lib)
target_link_libraries(nxsbuild PRIVATE metis GKlib)

install(TARGETS nxsbuild DESTINATION bin)
