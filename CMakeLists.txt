cmake_minimum_required(VERSION 3.25)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED 23)

set(CMAKE_CXX_COMPILER "/usr/bin/clang++")

project(engine)

find_package(fmt REQUIRED)
find_package(Draco REQUIRED)
file(GLOB_RECURSE SOURCE_DIRS LIST_DIRECTORIES true "${CMAKE_SOURCE_DIR}/src/*")
file(GLOB_RECURSE INCLUDE_DIRS LIST_DIRECTORIES true "${CMAKE_SOURCE_DIR}/include/*")

set(INCLUDES "")
set(SOURCES "")

foreach(ITEM ${SOURCE_DIRS})
    if(NOT IS_DIRECTORY ${ITEM})
        list(APPEND SOURCES ${ITEM})
    endif()
endforeach()

foreach(ITEM ${INCLUDE_DIRS})
    if(IS_DIRECTORY ${ITEM})
        list(APPEND INCLUDES ${ITEM})
    endif()
endforeach()

list(APPEND INCLUDES "${CMAKE_SOURCE_DIR}/include/")

message(STATUS "Sources found: ${SOURCES}")
message(STATUS "Include directories found: ${INCLUDES}")

message(STATUS "IMGUI sources: ${IMGUI_SOURCES}")
include_directories(./include ${INCLUDES})

add_executable(${PROJECT_NAME} ${SOURCES} ${IMGUI_SOURCES})

add_subdirectory(third/vk-bootstrap)

target_link_libraries(${PROJECT_NAME} PRIVATE assimp fmt vulkan vk-bootstrap::vk-bootstrap glm glfw tinygltf draco)

