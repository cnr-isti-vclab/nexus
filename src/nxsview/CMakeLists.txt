#export CMAKE_PREFIX_PATH=path to Qt/5.5/gcc_64/ (save for bin/qmake)

cmake_minimum_required(VERSION 3.1)
project(nxsview)

SET(HEADERS
	${VCGDIR}/wrap/gcache/token.h
	${VCGDIR}/wrap/gcache/provider.h
	${VCGDIR}/wrap/gcache/door.h
	${VCGDIR}/wrap/gcache/dheap.h
	${VCGDIR}/wrap/gcache/controller.h
	${VCGDIR}/wrap/gcache/cache.h
	../common/nexus.h
	../common/token.h
	../common/renderer.h
	../common/ram_cache.h
	../common/metric.h
	../common/gpu_cache.h
	../common/globalgl.h
	../common/frustum.h
	../common/dag.h
	../common/controller.h
	../nxszip/meshcoder.h
	../nxszip/cstream.h
	../nxszip/zpoint.h
	gl_nxsview.h
	scene.h
)

SET(SOURCES
	${VCGDIR}/wrap/gui/trackmode.cpp
	${VCGDIR}/wrap/gui/trackball.cpp
	${VCGDIR}/wrap/system/qgetopt.cpp
	../common/controller.cpp
	../common/nexus.cpp
	../common/renderer.cpp
	../common/ram_cache.cpp
	../common/frustum.cpp
	../common/qtnexusfile.cpp
	main.cpp
	gl_nxsview.cpp
	scene.cpp
)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5OpenGL)

include(${CMAKE_CURRENT_SOURCE_DIR}/glew.cmake)

add_executable(nxsview MACOSX_BUNDLE ${SOURCES} ${HEADERS})
if (WIN32)
	target_link_libraries(nxsview PUBLIC nexus Qt5::OpenGL external-glew glu32 OpenGL32)
else()
	target_link_libraries(nxsview PUBLIC nexus Qt5::OpenGL external-glew OpenGL::GLU OpenGL::GL)
endif()

target_compile_definitions(nxsview PRIVATE GL_COMPATIBILITY)

if (APPLE)
	set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
	set(CMAKE_INSTALL_RPATH $ORIGIN/../Frameworks)
	set_target_properties(nxsview PROPERTIES
		INSTALL_RPATH @executable_path/../Frameworks
		MACOSX_BUNDLE_BUNDLE_VERSION "${NEXUS_VERSION}"
		MACOSX_BUNDLE_SHORT_VERSION_STRING "${NEXUS_VERSION}"
		MACOSX_BUNDLE_INFO_STRING "nxsview ${NEXUS_VERSION}"
		MACOSX_BUNDLE_COPYRIGHT "Copyright VCG-ISTI-CNR © 2012-2021. All rights reserved."
		)
	
	if (BUILD_NXS_BUILD)
		set_target_properties(nxsbuild PROPERTIES
			INSTALL_RPATH @executable_path/../Frameworks)
		
		install(TARGETS nxsbuild DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/nxsview.app/Contents/MacOS/)
	endif()
	
	if (BUILD_NXS_EDIT)
		set_target_properties(nxsedit PROPERTIES
			INSTALL_RPATH @executable_path/../Frameworks)
		set_target_properties(nxscompress PROPERTIES
			INSTALL_RPATH @executable_path/../Frameworks)
		
		install(TARGETS nxsedit nxscompress DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/nxsview.app/Contents/MacOS/)
	endif()
	
	set_additional_settings_info_plist(
		TARGET nxsview
		FILE ${CMAKE_CURRENT_BINARY_DIR}/nxsview.app/Contents/Info.plist)
	
	install(TARGETS external-glew DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/nxsview.app/Contents/Frameworks)
	install (TARGETS nxsview DESTINATION ${CMAKE_INSTALL_PREFIX})
else()
	install (TARGETS nxsview)
endif()
