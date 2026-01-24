cmake_minimum_required(VERSION 3.10)
project(nxsbuild)

set(SOURCES
	main.cpp
	build_parameters.cpp
	merge_simplify_split.cpp
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

set(HEADERS
	build_parameters.h
	merge_simplify_split.h
	../core/mesh.h
	../core/spatial_sort.h
	../core/adjacency.h
	../core/clustering.h
	../core/clustering_metis.h
	../core/micro_clustering.h
	../core/micro_clustering_metis.h
	../loaders/meshloader.h
	../loaders/objexporter.h
	../loaders/plyexporter.h
)

add_executable(nxsbuild ${SOURCES} ${HEADERS})

# Find Qt5
find_package(Qt5 COMPONENTS Core REQUIRED)

# Add METIS library paths
target_include_directories(nxsbuild PRIVATE 
    ../core
    /home/ponchio/local/include
)

target_link_directories(nxsbuild PRIVATE /home/ponchio/local/lib)
target_link_libraries(nxsbuild PRIVATE metis GKlib Qt5::Core)

install(TARGETS nxsbuild DESTINATION bin)
