cmake_minimum_required(VERSION 3.10)

project(orbiter)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_C_STANDARD 11)

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
	src/common/string-utils.c
)

set(SERVER_SRCS
	src/orbiter-server.c
	src/media/video-capture/video-capture.c
)
if(WIN32)
	list(APPEND SERVER_SRCS 
		src/media/common/win/mf-toolkit.c
		src/media/common/win/d3d11-toolkit.c
		src/media/common/win/dxgi-toolkit.c
		src/media/video-capture/platform/win/platform-video-capture.c
	)
endif()

add_executable(orbiter-server ${SERVER_SRCS} ${COMMON_SRCS})
target_link_libraries(orbiter-server PUBLIC Threads::Threads cdk ${OPENSSL_LIBRARIES})
