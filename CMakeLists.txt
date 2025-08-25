cmake_minimum_required(VERSION 3.5)
project(Simple3DViewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt installation path
set(Qt6_DIR  "D:/.CODING/QtFramework/6.9.1/msvc2022_64/lib/cmake/Qt6")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Widgets)

# Find DirectX 12 (Windows-specific)
find_library(D3D12_LIBRARY d3d12)
find_library(DXGI_LIBRARY dxgi)
find_library(D3DCompiler_LIBRARY d3dcompiler)

include_directories(include)

add_executable(Simple3DViewer
	src/main.cpp
	src/MainWindow.cpp
	src/D3D12Viewport.cpp
	src/Model.cpp
	include/MainWindow.h
	include/D3D12Viewport.h
	include/Model.h
 )
target_link_libraries(Simple3DViewer PRIVATE
	Qt6::Widgets
	${D3D12_LIBRARY}
	${DXGI_LIBRARY}
	${D3DCompiler_LIBRARY}
)

# Post-build step to copy Qt DLLs (Windows-specific)
if(WIN32 AND Qt6_FOUND)
	# Find windeploy tool
	get_target_property(Qt6_qmake_location Qt6::qmake IMPORTED_LOCATION)
	get_filename_component(Qt6_bind_dir "${Qt6_qmake_location}" DIRECTORY)
	set(WINDEPLOYQT "${Qt6_bind_dir}/windeployqt.exe")

	if (EXISTS "${WINDEPLOYQT}")
		add_custom_command(TARGET Simple3DViewer POST_BUILD
			COMMAND "${WINDEPLOYQT}" "$<TARGET_FILE:Simple3DViewer>" --no-compiler-runtime --no-system-d3d-compiler
			COMMENT "Running windeployqt to bundle Qt dependencies..."
		)
	endif()
endif()

# Include directories for headers
target_include_directories(Simple3DViewer PRIVATE include)