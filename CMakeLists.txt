cmake_minimum_required(VERSION 3.10)

set(SDK_PATH $ENV{LUCKFOX_SDK_PATH})

if (NOT SDK_PATH)
    message(WARNING "Please Set Luckfox-pico SDK Path. Such as:export LUCKFOX_SDK_PATH=/home/user/luckfox-pico")
    return()
else()
    set(CMAKE_C_COMPILER "${SDK_PATH}/tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf/bin/arm-rockchip830-linux-uclibcgnueabihf-gcc")
    set(CMAKE_CXX_COMPILER "${SDK_PATH}/tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf/bin/arm-rockchip830-linux-uclibcgnueabihf-g++")
    set(CMAKE_CXX_STANDARD 11)
    set(ENV{PKG_CONFIG_SYSROOT_DIR} "${SDK_PATH}/sysdrv/out/rootfs_uclibc_rv1106")
    set(ENV{PKG_CONFIG_PATH} "${SDK_PATH}/sysdrv/source/buildroot/buildroot-2023.02.6/output/host/arm-buildroot-linux-uclibcgnueabihf/sysroot/usr/lib/pkgconfig")
endif()

project(webstream)

set(CMAKE_INSTALL_RPATH "$ORIGIN/lib")
add_definitions(-DRV1106_1103)
add_definitions(-DASIO_STANDALONE)

link_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib/uclibc)

#Opencv 4
# set(OpenCV_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib/uclibc/lib/cmake/opencv4")
set(OpenCV_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib/opencv-mobile-4.10.0-luckfox-pico/lib/cmake/opencv4")
find_package(OpenCV REQUIRED)

#Thread
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# OpenSSL
set(OPENSSL_LIB_DIR "${CMAKE_SOURCE_DIR}/lib/uclibc")
set(OPENSSL_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")

# Gstreamer
find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED gstreamer-1.0)
pkg_check_modules(GST_APP REQUIRED gstreamer-app-1.0)

set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
file(GLOB SRC_FILES "${SRC_DIR}/src/*.cpp")
add_executable(${PROJECT_NAME} ${SRC_FILES})

add_compile_options(-g -Wall
                    -DISP_HW_V30 -DRKPLATFORM=ON -DARCH64=OFF
                    -DROCKIVA -DUAPI2
                    -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 
                    )

set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath-link,${CMAKE_CURRENT_SOURCE_DIR}/lib:/usr/lib")

target_link_directories(${PROJECT_NAME} PRIVATE
    ${SDK_PATH}/sysdrv/out/rootfs_uclibc_rv1106/lib
    ${SDK_PATH}/sysdrv/out/rootfs_uclibc_rv1106/usr/lib
)

target_link_libraries(${PROJECT_NAME}
                    ${OpenCV_LIBS}
                    rknnmrt
                    Threads::Threads
                    rockiva
                    sample_comm
                    rockit
                    rockchip_mpp
                    rkaiq
                    pthread
                    rtsp
                    rga
                    ${OPENSSL_LIB_DIR}/libssl.a
                    ${OPENSSL_LIB_DIR}/libcrypto.a
                    ${GST_LIBRARIES}
                    ${GST_APP_LIBRARIES}
                    intl
                    iconv
                    ffi
                    pcre
                    gmodule-2.0
                    )


target_include_directories(${PROJECT_NAME} PRIVATE                        
                            ${OpenCV_INCLUDE_DIRS}
                            ${GST_INCLUDE_DIRS}
                            ${GST_APP_INCLUDE_DIRS}
                            ${SDK_PATH}/sysdrv/source/buildroot/buildroot-2023.02.6/output/host/arm-buildroot-linux-uclibcgnueabihf/sysroot/usr/include/gstreamer-1.0
                            ${SDK_PATH}/sysdrv/source/buildroot/buildroot-2023.02.6/output/host/arm-buildroot-linux-uclibcgnueabihf/sysroot/usr/include/glib-2.0
                            ${SDK_PATH}/sysdrv/source/buildroot/buildroot-2023.02.6/output/host/arm-buildroot-linux-uclibcgnueabihf/sysroot/usr/lib/glib-2.0/include
                            src/include
                            ${PROJECT_SOURCE_DIR}/lib/asio/asio/include
                            ${PROJECT_SOURCE_DIR}/lib/websocketpp
                            ${CMAKE_CURRENT_SOURCE_DIR}
                            ${CMAKE_CURRENT_SOURCE_DIR}/utils
                            ${CMAKE_CURRENT_SOURCE_DIR}/common 
                            ${CMAKE_CURRENT_SOURCE_DIR}/common/isp3.x   
                            ${CMAKE_CURRENT_SOURCE_DIR}/include
                            ${CMAKE_CURRENT_SOURCE_DIR}/include/openssl
                            ${CMAKE_CURRENT_SOURCE_DIR}/include/rknn
                            ${CMAKE_CURRENT_SOURCE_DIR}/include/librga 
                            ${CMAKE_CURRENT_SOURCE_DIR}/include/rkaiq
                            ${CMAKE_CURRENT_SOURCE_DIR}/include/rkaiq/uAPI2
                            ${CMAKE_CURRENT_SOURCE_DIR}/include/rkaiq/common
                            ${CMAKE_CURRENT_SOURCE_DIR}/include/rkaiq/xcore
                            ${CMAKE_CURRENT_SOURCE_DIR}/include/rkaiq/algos
                            ${CMAKE_CURRENT_SOURCE_DIR}/include/rkaiq/iq_parser
                            ${CMAKE_CURRENT_SOURCE_DIR}/include/rkaiq/iq_parser_v2
                            ${CMAKE_CURRENT_SOURCE_DIR}/include/rkaiq/smartIr
                          )
