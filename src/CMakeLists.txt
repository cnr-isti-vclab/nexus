cmake_minimum_required(VERSION 3.10)

add_library(nexus
	core/basemesh.cpp
	core/cone.cpp
	core/mapped_file.cpp
	core/mappedmesh.cpp
	core/material.cpp
	core/nexusdata.cpp
	core/nodemesh.cpp
	core/qtnexusfile.cpp
	core/signature.cpp
	core/traversal.cpp
	core/virtualarray.cpp
)

target_compile_features(nexus PUBLIC cxx_std_20)
target_include_directories(nexus PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core>
	$<BUILD_INTERFACE:${VCGDIR}>
	$<BUILD_INTERFACE:${VCGDIR}/eigenlib>
	$<INSTALL_INTERFACE:include/nexus>
)
target_link_libraries(nexus PUBLIC Qt6::Core Qt6::Gui Threads::Threads corto)
if(TARGET vcglib::vcglib)
	target_link_libraries(nexus PUBLIC vcglib::vcglib)
endif()

install(TARGETS nexus EXPORT nexusTargets
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
	RUNTIME DESTINATION bin
)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/core/"
	DESTINATION include/nexus
	FILES_MATCHING PATTERN "*.h"
)
