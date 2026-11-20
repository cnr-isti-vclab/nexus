find_package(Qt6 6.7 REQUIRED COMPONENTS Widgets GuiPrivate ShaderTools)
find_package(Threads REQUIRED)

set(CMAKE_AUTOMOC ON)

set(RHI_HEADERS
	rhi_nxsview.h
	rhi_renderer.h
	../nxsview/nxsview.h
	../nxsview/scene.h
	../common/renderer.h
	../common/metric.h
	../common/controller.h
	../common/nexus.h
	../common/nexusdata.h
	../common/traversal.h
	../rhi/gpu_cache_allocator.h
)

set(RHI_SOURCES
	main.cpp
	rhi_nxsview.cpp
	rhi_renderer.cpp
	../nxsview/nxsview.cpp
	../nxsview/scene.cpp
	../common/controller.cpp
	../common/nexus.cpp
	../common/nexusdata.cpp
	../common/qtnexusfile.cpp
	../common/ram_cache.cpp
	../common/frustum.cpp
	../common/renderer.cpp
	../common/traversal.cpp
	../common/cone.cpp
	../rhi/gpu_cache_allocator.cpp
)

add_executable(nxsview_rhi MACOSX_BUNDLE ${RHI_HEADERS} ${RHI_SOURCES})
target_compile_features(nxsview_rhi PRIVATE cxx_std_20)
target_compile_definitions(nxsview_rhi PRIVATE NX_RHI)
target_include_directories(nxsview_rhi SYSTEM PRIVATE
	"${CMAKE_CURRENT_LIST_DIR}/../../../vcglib"
	"${CMAKE_CURRENT_LIST_DIR}/../corto/include"
)

set(CORTO_LIBRARY "${CMAKE_CURRENT_LIST_DIR}/../corto/libcorto.a")
target_link_libraries(nxsview_rhi PRIVATE
	Qt6::Widgets
	Qt6::GuiPrivate
	Threads::Threads
	"${CORTO_LIBRARY}"
)

qt_add_shaders(nxsview_rhi nxsview_rhi_shaders
	PREFIX "/nxsview_rhi"
	BASE "${CMAKE_CURRENT_LIST_DIR}"
	FILES
		shaders/mesh.vert
		shaders/mesh.frag
)