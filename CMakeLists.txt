#export CMAKE_PREFIX_PATH=path to Qt/5.5/gcc_64/ (where you would find bin/qmake)
cmake_minimum_required(VERSION 3.10)
project(nexus)

option(BUILD_NXS_BUILD "" ON)
option(BUILD_NXS_EDIT "" ON)
option(BUILD_NXS_VIEW "" ON)

set (CMAKE_CXX_STANDARD 11)

# enable setting options with SET cmake command
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

find_package(Qt5 COMPONENTS Widgets REQUIRED)

if (BUILD_NXS_VIEW)
	include(${CMAKE_CURRENT_SOURCE_DIR}/src/cmake/tools.cmake)
endif()

# nexus version
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/NEXUS_VERSION")
	file (READ "${CMAKE_CURRENT_SOURCE_DIR}/NEXUS_VERSION" NEXUS_VERSION)
else()
	message(FATAL_ERROR "NEXUS_VERSION file not found in the repository")
endif()

# VCGLib -- required
find_package(vcglib CONFIG)
if(TARGET vcglib::vcglib)
	message(STATUS "nexus - VCGLib - using system provided library")
	add_library(vcglib ALIAS vcglib::vcglib)
	get_target_property(VCGDIR vcglib::vcglib INTERFACE_INCLUDE_DIRECTORIES)
elseif (VCGDIR) # VCGDIR exists - using custom user vcglib path
	if(EXISTS ${VCGDIR})
		add_subdirectory(${VCGDIR} ${CMAKE_CURRENT_BINARY_DIR}/vcglib)
		message(STATUS "nexus - VCGLib - using custom VCGDIR path library")
	else()
		set(VCGDIR NOTFOUND)
	endif()
else()
	get_filename_component(VCGDIR "${CMAKE_CURRENT_LIST_DIR}/../vcglib" ABSOLUTE)
	if(EXISTS ${VCGDIR})
		add_subdirectory(${VCGDIR} ${CMAKE_CURRENT_BINARY_DIR}/vcglib)
		message(STATUS "nexus - VCGLib - using bundled source")
	else()
		set(VCGDIR NOTFOUND)
	endif()
endif()
set(VCGDIR "${VCGDIR}")

if(NOT VCGDIR)
	message(FATAL_ERROR "VCGLib not found.")
endif()

#corto options
set(BUILD_CORTO_CODEC_UNITY OFF)
set(BUILD_CORTO_EXE OFF)

find_package(corto CONFIG)
if(TARGET corto::corto)
	message(STATUS "nexus - corto - using system provided library")
	add_library(corto ALIAS corto::corto)
else()
	add_subdirectory(src/corto EXCLUDE_FROM_ALL)
endif()

add_subdirectory(src)

if (BUILD_NXS_BUILD)
	add_subdirectory(src/nxsbuild)
endif()

if(BUILD_NXS_EDIT)
	add_subdirectory(src/nxsedit)
endif()

if(BUILD_NXS_VIEW)
	add_subdirectory(src/nxsview)
endif()

if(TARGET vcglib::vcglib AND TARGET corto::corto)
	install(EXPORT nexusTargets
		DESTINATION lib/cmake/nexus
		NAMESPACE nexus::
		FILE nexusTargets.cmake
	)

	include(CMakePackageConfigHelpers)
	# generate the config file that includes the exports
	configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
		"${CMAKE_CURRENT_BINARY_DIR}/nexusConfig.cmake"
		INSTALL_DESTINATION lib/cmake/nexus
		NO_SET_AND_CHECK_MACRO
		NO_CHECK_REQUIRED_COMPONENTS_MACRO
	)

	install(FILES
		"${CMAKE_CURRENT_BINARY_DIR}/nexusConfig.cmake"
		DESTINATION lib/cmake/nexus
	)
endif()
