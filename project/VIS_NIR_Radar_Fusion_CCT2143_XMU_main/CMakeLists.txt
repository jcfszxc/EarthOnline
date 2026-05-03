cmake_minimum_required(VERSION 3.10)
project(VIS_NIR_Radar_Fusion_CCT2143_XMU)

# 交叉编译设置
message(STATUS "Use aarch64-linux- (RK3588) cross compiler")
set(CROSS_COMPILER_PATH "aarch64-linux-")
set(CMAKE_SYSTEM_NAME "Linux")
set(CMAKE_CXX_COMPILER "${CROSS_COMPILER_PATH}g++")
set(CMAKE_C_COMPILER "${CROSS_COMPILER_PATH}gcc")

IF(CMAKE_SYSTEM_NAME MATCHES "Windows")
add_definitions(-DWIN64)
ELSEIF(CMAKE_SYSTEM_NAME MATCHES "Linux")
add_definitions(-DLINUX)
ENDIF()

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 编译标志
set(COMPILE_FLAG "-Wall -lm -ldl -lstdc++ -lpthread -fPIC -fno-aggressive-loop-optimizations -ffunction-sections -fstack-protector -fshort-wchar -Wno-date-time -O2")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMPILE_FLAG}")

# 链接标志
MATH(EXPR stacksize "1*1024*1024")
set(CMAKE_LD_FLAGS "-fno-aggressive-loop-optimizations -fstack-protector -Wl,-z,relro -Wl,-z,noexecstack -fPIC -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -ldl -Wl,--stack,${stacksize}")

# 设置 sysroot
set(SDK_SYS_ROOT $ENV{RK_SYSROOT})
if(NOT SDK_SYS_ROOT)
    message(FATAL_ERROR "sysroot is not defined, please install RK SDK.")
endif()
set(CMAKE_SYSROOT ${SDK_SYS_ROOT})

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(PkgConfig REQUIRED)

# 添加opencv依赖库
pkg_check_modules(OPENCV REQUIRED opencv4)
include_directories(${OPENCV_INCLUDE_DIRS})
link_directories(${OPENCVLIBRARY_DIRS})

# RGA 库设置
set(RGA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/runtime/rkrga")
set(RGA_INCLUDE_DIR "${RGA_DIR}/include")
set(RGA_LIBRARY_DIR "${RGA_DIR}/lib")
set(RGA_LIBRARY "${RGA_LIBRARY_DIR}/librga.so")

# 添加 RGA 头文件目录
include_directories(${RGA_INCLUDE_DIR})

# 查找gstreamer 库
pkg_check_modules(GST REQUIRED IMPORTED_TARGET
    gstreamer-1.0
    gobject-2.0
    gstreamer-rtsp-server-1.0
    gstreamer-plugins-base-1.0
    gstreamer-plugins-bad-1.0
    gstreamer-app-1.0
)

# RKNN API setup
set(RKNN_API_PATH "${CMAKE_CURRENT_SOURCE_DIR}/runtime/librknn_api")
set(RKNN_RT_LIB "${RKNN_API_PATH}/aarch64/librknnrt.so")

# 添加 libcc_rtspserver.so
set(CC_RTSPSERVER_LIB "${CMAKE_CURRENT_SOURCE_DIR}/runtime/libcc_rtspserver.so")

# 添加头文件目录
include_directories(
    ${RKNN_API_PATH}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${GST_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime 
)

# 添加源文件
file(GLOB SOURCES 
    "src/*.cpp"
)

# 创建可执行文件
add_executable(${PROJECT_NAME} ${SOURCES})

# 链接必要的库
target_link_libraries(${PROJECT_NAME}
    pthread
    ${GST_LIBRARIES}
    ${OPENCV_LIBRARIES}
    ${RGA_LIBRARY}
    ${RKNN_RT_LIB}
    ${CC_RTSPSERVER_LIB}
)

# 设置 rpath
set_target_properties(${PROJECT_NAME} PROPERTIES
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "$ORIGIN/../lib"
)

# 安装目标
install(TARGETS ${PROJECT_NAME} DESTINATION bin)