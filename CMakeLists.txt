#export CMAKE_PREFIX_PATH=path to Qt/5.5/gcc_64/ (where you would find bin/qmake)
cmake_minimum_required(VERSION 3.1)
project(nexus)

option(BUILD_NXS_BUILD "" ON)
option(BUILD_NXS_EDIT "" ON)
option(BUILD_NXS_VIEW "" ON)

set (CMAKE_CXX_STANDARD 11)

# enable setting options with SET cmake command
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

find_package(Qt5 COMPONENTS Core Gui REQUIRED)

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
if (VCGDIR) # VCGDIR exists - using custom user vcglib path
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

add_subdirectory(src/corto EXCLUDE_FROM_ALL)

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
