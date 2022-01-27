# Copyright 2019, 2020, Collabora, Ltd.
# Copyright 2019, 2020, Visual Computing Lab, ISTI - Italian National Research Council
# SPDX-License-Identifier: BSL-1.0

#if both options are set to on, priority is given to system glew
option(NEXUS_USES_BUNDLED_GLEW "Allow use of bundled GLEW source" ON)
option(NEXUS_USES_SYSTEM_GLEW "Allow use of system-provided GLEW" ON)

find_package(OpenGL)
find_package(GLEW)

unset(HAVE_SYSTEM_GLEW)
if(DEFINED GLEW_VERSION)
	if((TARGET GLEW::GLEW) AND (${GLEW_VERSION} VERSION_GREATER_EQUAL "2.0.0"))
		set(HAVE_SYSTEM_GLEW TRUE)
	endif()
endif()

if(NEXUS_USES_SYSTEM_GLEW AND HAVE_SYSTEM_GLEW)
	message(STATUS "- glew - using system-provided library")
	add_library(external-glew INTERFACE)
	target_link_libraries(external-glew INTERFACE GLEW::GLEW)
	if(TARGET OpenGL::GL)
		target_link_libraries(external-glew INTERFACE OpenGL::GL)
	elseif(TARGET OpenGL::OpenGL)
		target_link_libraries(external-glew INTERFACE OpenGL::OpenGL)
	else()
		message(FATAL_ERROR "OpenGL not found or your CMake version is too old!")
	endif()
elseif(NEXUS_USES_BUNDLED_GLEW)
	set(GLEW_DIR ${CMAKE_CURRENT_LIST_DIR}/glew-2.1.0)
	message(STATUS "- glew - using bundled source")
	add_library(external-glew SHARED "${GLEW_DIR}/src/glew.c")
	target_include_directories(external-glew SYSTEM PUBLIC ${GLEW_DIR}/include)
	if(TARGET OpenGL::GL)
		target_link_libraries(external-glew PUBLIC OpenGL::GL)
	elseif(TARGET OpenGL::OpenGL)
		target_link_libraries(external-glew PUBLIC OpenGL::OpenGL)
	else()
		message(FATAL_ERROR "OpenGL not found or your CMake version is too old!")
	endif()
	if(TARGET OpenGL::GLX)
		target_link_libraries(external-glew PUBLIC OpenGL::GLX)
	endif()

	install(TARGETS external-glew)
elseif(NOT TARGET external-glew)
	message(STATUS "Glew seems to be missing. Please enable NEXUS_USES_BUNDLED_GLEW option.")
endif()
