cmake_minimum_required(VERSION 3.21)
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
	message(FATAL_ERROR "In-source builds are not allowed.")
endif()

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED True)
project(CArkanoid VERSION 0.0.1 DESCRIPTION "The clone of game Arkanoid written in C." LANGUAGES C)

if(UNIX AND NOT APPLE)
	find_package(PkgConfig REQUIRED)

	pkg_search_module(SDL2 REQUIRED sdl2)
	set(SDL2_INCLUDE_PATH ${SDL2_INCLUDE_DIRS})
    set(SDL2_LIBRARY_PATH ${SDL2_LIBRARIES})
	pkg_search_module(SDL2IMAGE REQUIRED SDL2_image)
	set(SDL2_IMAGE_INCLUDE_PATH ${SDL2IMAGE_INCLUDE_DIRS})
    set(SDL2_IMAGE_LIBRARY_PATH ${SDL2IMAGE_LIBRARIES})

	find_package(Vulkan REQUIRED)
	set(VULKAN_SDK_INCLUDE_PATH ${Vulkan_INCLUDE_DIRS})
	set(VULKAN_SDK_LIBRARY_PATH ${Vulkan_LIBRARIES})
	pkg_search_module(Shaderc REQUIRED shaderc)
elseif(MSVC)
	message("Searching for SDL2")
	find_path(SDL2_FIND_PATH NAMES "include/SDL.h")
	if(SDL2_FIND_PATH STREQUAL "")
			message(FATAL_ERROR "Couldn't find SDL2 library.")
	endif()
	set(SDL2_INCLUDE_PATH "${SDL2_FIND_PATH}/include")
	if(${CMAKE_SIZEOF_VOID_P} MATCHES 8)
		set(SDL2_LIBRARY_PATH "${SDL2_FIND_PATH}/lib/x64/SDL2.lib;${SDL2_FIND_PATH}/lib/x64/SDL2main.lib")
		set(SDL2_OUTPUT_DLL "${SDL2_FIND_PATH}/lib/x64/SDL2.dll")
	else()
		set(SDL2_LIBRARY_PATH "${SDL2_FIND_PATH}/lib/x86/SDL2.lib;${SDL2_FIND_PATH}/lib/x86/SDL2main.lib")
		set(SDL2_OUTPUT_DLL "${SDL2_FIND_PATH}/lib/x86/SDL2.dll")
	endif()

	message("Searching for SDL2_image")
	find_path(SDL2_IMAGE_FIND_PATH NAMES "include/SDL_image.h")
	if(SDL2_IMAGE_FIND_PATH STREQUAL "")
			message(FATAL_ERROR "Couldn't find SDL2_image library.")
	endif()
	set(SDL2_IMAGE_INCLUDE_PATH "${SDL2_IMAGE_FIND_PATH}/include")
	if(${CMAKE_SIZEOF_VOID_P} MATCHES 8)
		set(SDL2_IMAGE_LIBRARY_PATH "${SDL2_IMAGE_FIND_PATH}/lib/x64/SDL2_image.lib")
		set(SDL2_IMAGE_OUTPUT_DLL "${SDL2_IMAGE_FIND_PATH}/lib/x64/SDL2_image.dll;${SDL2_IMAGE_FIND_PATH}/lib/x64/zlib1.dll;${SDL2_IMAGE_FIND_PATH}/lib/x64/libpng16-16.dll")
	else()
		set(SDL2_IMAGE_LIBRARY_PATH "${SDL2_IMAGE_FIND_PATH}/lib/x86/SDL2_image.lib")
		set(SDL2_IMAGE_OUTPUT_DLL "${SDL2_IMAGE_FIND_PATH}/lib/x86/SDL2_image.dll;${SDL2_IMAGE_FIND_PATH}/lib/x86/zlib1.dll;${SDL2_IMAGE_FIND_PATH}/lib/x86/libpng16-16.dll")
	endif()

	message("Searching for Vulkan SDK")
	if(DEFINED ENV{VK_SDK_PATH})
		message("Found Vulkan SDK: ${VK_SDK_PATH}")
		set(VULKAN_SDK_INCLUDE_PATH $ENV{VK_SDK_PATH}/Include)
		set(VULKAN_SDK_LIBRARY_PATH "$ENV{VK_SDK_PATH}/Lib/vulkan-1.lib")
	elseif(DEFINED ENV{VULKAN_SDK})
		message("Found Vulkan SDK: ${VULKAN_SDK}")
		set(VULKAN_SDK_INCLUDE_PATH $ENV{VULKAN_SDK}/Include)
		set(VULKAN_SDK_LIBRARY_PATH "$ENV{VK_SDK_PATH}/Lib32/vulkan-1.lib")
	else()
		message(FATAL_ERROR "Vulkan SDK is not installed or environmental variables VK_SDK_PATH/VULKAN_SDK are not set.")
	endif()
else()
	message(FATAL_ERROR "Unsupported operating system.")
endif()

include_directories(${SDL2_INCLUDE_PATH})
include_directories(${SDL2_IMAGE_INCLUDE_PATH})
include_directories(${VULKAN_SDK_INCLUDE_PATH})
add_executable(Game main.c main.h math.h math.c engine.h engine.c renderer.h renderer.c vulkan.h vulkan.c vulkan_buffer.h vulkan_buffer.c vulkan_image.h vulkan_image.c quit.h quit.c)
set_target_properties(Game PROPERTIES LINKER_LANGUAGE C)
target_link_libraries(Game ${SDL2_LIBRARY_PATH})
target_link_libraries(Game ${SDL2_IMAGE_LIBRARY_PATH})
target_link_libraries(Game ${VULKAN_SDK_LIBRARY_PATH})

if(NOT MSVC)
    target_link_libraries(Game m)
endif()

if(MSVC)
	if(${CMAKE_SIZEOF_VOID_P} MATCHES 8)
		target_link_libraries(Game "${VULKAN_SDK_INCLUDE_PATH}/../lib/$<IF:$<CONFIG:DEBUG>,shaderc_combinedd.lib,shaderc_combined.lib>")
	else()
		target_link_libraries(Game "${VULKAN_SDK_INCLUDE_PATH}/../lib32/$<IF:$<CONFIG:DEBUG>,shaderc_combinedd.lib,shaderc_combined.lib>")
	endif()
else()
	target_link_libraries(Game ${Shaderc_LIBRARIES})
endif()

target_compile_definitions(Game PUBLIC $<$<CONFIG:DEBUG>:DEBUG_BUILD>)
target_compile_options(Game PRIVATE
  $<$<C_COMPILER_ID:MSVC>:/Zc:preprocessor /permissive- /W4 /Wall /wd4820 /wd5045>
  $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wall -Wextra -pedantic>
)

add_custom_command(
	TARGET Game POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E remove_directory
	"$<TARGET_FILE_DIR:Game>/assets"
)
add_custom_command(
	TARGET Game POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
	"${CMAKE_SOURCE_DIR}/assets"
	"$<TARGET_FILE_DIR:Game>/assets"
)

if(MSVC)
	set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT Game)
	set_target_properties(Game PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY $<TARGET_FILE_DIR:Game>)
	set_target_properties(Game PROPERTIES VS_DEBUGGER_COMMAND $<TARGET_FILE:Game>)
	set_target_properties(Game PROPERTIES VS_DEBUGGER_ENVIRONMENT "PATH=%PATH%;${CMAKE_PREFIX_PATH}/bin")
	set_target_properties(Game PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS")

	add_custom_command(
		TARGET Game POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
		${SDL2_OUTPUT_DLL}
		$<TARGET_FILE_DIR:Game>
	)
	add_custom_command(
		TARGET Game POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
		${SDL2_IMAGE_OUTPUT_DLL}
		$<TARGET_FILE_DIR:Game>
	)
endif()