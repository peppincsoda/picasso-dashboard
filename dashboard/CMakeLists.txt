cmake_minimum_required(VERSION 3.9)

project(dashboard LANGUAGES CXX)

file(GLOB SRCS *.cpp *.qrc)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 COMPONENTS Core Quick SerialPort REQUIRED)

add_executable(${PROJECT_NAME} ${SRCS})

target_include_directories(${PROJECT_NAME} PUBLIC ${MY_ROOT_DIR})

target_link_libraries(${PROJECT_NAME} Qt5::Core Qt5::Quick Qt5::SerialPort)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
