cmake_minimum_required(VERSION 3.10)
project(nxsview)

find_package(Qt6 6.7 REQUIRED COMPONENTS Widgets OpenGLWidgets GuiPrivate ShaderTools)
find_package(Threads REQUIRED)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

include(${CMAKE_CURRENT_SOURCE_DIR}/renderers/opengl/glew.cmake)

set(VIEW_HEADERS
	controller.h
	frustum.h
	globalgl.h
	gpu_cache.h
	metric.h
	nexus.h
	nxsview.h
	prioritizer.h
	ram_cache.h
	renderer.h
	scene.h
	token.h
	trackball.h
	../src/core/traversal.h
	renderers/opengl/gl_nxsview.h
	renderers/opengl/gl_renderer.h
	renderers/qrhi/rhi_nxsview.h
	renderers/qrhi/rhi_renderer.h
	renderers/qrhi/gpu_cache_allocator.h
	renderers/qrhi/qrhi_window.h
	gui/trackball.h
	gui/trackmode.h
	gui/trackutils.h
	gui/view.h
)

set(VIEW_SOURCES
	controller.cpp
	frustum.cpp
	main.cpp
	nexus.cpp
	nxsview.cpp
	ram_cache.cpp
	renderer.cpp
	scene.cpp
	renderers/opengl/gl_nxsview.cpp
	renderers/opengl/gl_renderer.cpp
	renderers/qrhi/rhi_nxsview.cpp
	renderers/qrhi/rhi_renderer.cpp
	renderers/qrhi/qrhi_window.cpp
	renderers/qrhi/gpu_cache_allocator.cpp
)

add_executable(nxsview MACOSX_BUNDLE ${VIEW_SOURCES} ${VIEW_HEADERS}
	nxsview.ui
)

target_compile_features(nxsview PRIVATE cxx_std_20)
target_compile_definitions(nxsview PRIVATE GL_COMPATIBILITY NXSVIEW_QRHI_RESOURCES)
target_include_directories(nxsview PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(nxsview SYSTEM PRIVATE
	"${VCGDIR}"
	"${VCGDIR}/eigenlib"
	"${CMAKE_CURRENT_LIST_DIR}/../src/corto/include"
)

target_link_libraries(nxsview PRIVATE
	nexus
	Qt6::Widgets
	Qt6::OpenGLWidgets
	Qt6::GuiPrivate
	Threads::Threads
	external-glew
)

if(WIN32)
	target_link_libraries(nxsview PRIVATE glu32 OpenGL32)
else()
	target_link_libraries(nxsview PRIVATE OpenGL::GLU OpenGL::GL)
endif()

qt_add_shaders(nxsview nxsview_qrhi_shaders
	PREFIX "/nxsview/qrhi"
	BASE "${CMAKE_CURRENT_LIST_DIR}/renderers/qrhi"
	FILES
		"${CMAKE_CURRENT_LIST_DIR}/renderers/qrhi/shaders/mesh.vert"
		"${CMAKE_CURRENT_LIST_DIR}/renderers/qrhi/shaders/mesh.frag"
)

if(APPLE)
	set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
	set(CMAKE_INSTALL_RPATH $ORIGIN/../Frameworks)
	set_target_properties(nxsview PROPERTIES
		INSTALL_RPATH @executable_path/../Frameworks
		MACOSX_BUNDLE_BUNDLE_VERSION "${NEXUS_VERSION}"
		MACOSX_BUNDLE_SHORT_VERSION_STRING "${NEXUS_VERSION}"
		MACOSX_BUNDLE_INFO_STRING "nxsview ${NEXUS_VERSION}"
		MACOSX_BUNDLE_COPYRIGHT "Copyright VCG-ISTI-CNR © 2012-2021. All rights reserved."
	)

	if(BUILD_NXS_BUILD)
		set_target_properties(nxsbuild PROPERTIES
			INSTALL_RPATH @executable_path/../Frameworks
		)
		install(TARGETS nxsbuild DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/nxsview.app/Contents/MacOS/)
	endif()

	if(BUILD_NXS_EDIT)
		set_target_properties(nxsedit nxscompress PROPERTIES
			INSTALL_RPATH @executable_path/../Frameworks
		)
		install(TARGETS nxsedit nxscompress DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/nxsview.app/Contents/MacOS/)
	endif()

	set_additional_settings_info_plist(
		TARGET nxsview
		FILE ${CMAKE_CURRENT_BINARY_DIR}/nxsview.app/Contents/Info.plist
	)

	install(TARGETS external-glew DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/nxsview.app/Contents/Frameworks)
	install(TARGETS nxsview DESTINATION ${CMAKE_INSTALL_PREFIX})
else()
	install(TARGETS nxsview)
endif()
