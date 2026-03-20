cmake_minimum_required(VERSION 3.16)
project(chip_image_detect VERSION 0.1 LANGUAGES CXX)


set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 解决Anaconda 冲突警告
set(CMAKE_IGNORE_PATH "/home/phynix/anaconda3/lib")

find_package(Qt6 REQUIRED COMPONENTS Widgets)

#收集所有文件
#GLOB_RECURSE 采用递归方式检索src中的所有文件
file(GLOB_RECURSE SRC_FILES 
    "src/*.cpp"
    "main.cpp"
)
file(GLOB_RECURSE HEADER_FILES 
    "include/*.h"
)
file(GLOB_RECURSE UI_FILES 
    "src/*.ui"
)

# 创建目标 
qt_add_executable(${PROJECT_NAME}
    MANUAL_FINALIZATION
    ${SRC_FILES}
    ${HEADER_FILES}
    ${UI_FILES}
)

# 配置目标属性
target_include_directories(${PROJECT_NAME} PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets)

#结束配置
qt_finalize_executable(${PROJECT_NAME})