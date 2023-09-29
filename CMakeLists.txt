cmake_minimum_required(VERSION 3.10)

project(orbiter)

option(BUILD_ORBITER_SERVER "build orbiter server" ON)
option(BUILD_ORBITER_CLIENT "build orbiter client" OFF)
option(BUILD_VIDEO_CAPTURE_MODULE "build video capture" ON)


set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 20)

if(WIN32)
add_compile_options("/experimental:c11atomics")
add_compile_options("/D_CRT_SECURE_NO_WARNINGS")
endif()

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

include_directories(include)
include_directories(src)
include_directories(third-party/cdk/include)
include_directories(${OPENSSL_INCLUDE_DIR})

link_directories(third-party/cdk/lib)

set(COMMON_SRCS
	src/common/synchronized-queue.c
)

set(VIDEO_CAPTURE_MODULE_SRCS
	src/media/video-capture/video-capture.c
)
if(WIN32)
	list(APPEND VIDEO_CAPTURE_MODULE_SRCS 
		src/media/video-capture/platform/win/dxgi-video-capture.c
		src/media/video-capture/platform/win/platform-video-capture.c
	)
endif()

if(BUILD_ORBITER_SERVER)
	set(SRCS src/orbiter-server.c ${COMMON_SRCS})
	if(BUILD_VIDEO_CAPTURE_MODULE)
		list(APPEND SRCS ${VIDEO_CAPTURE_MODULE_SRCS})
	endif()
endif()

if(BUILD_ORBITER_CLIENT)
	set(SRCS src/orbiter-client.c)
endif()

add_executable(orbiter-server ${SRCS})
target_link_libraries(orbiter-server PUBLIC Threads::Threads cdk ${OPENSSL_LIBRARIES})
