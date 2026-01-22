cmake_minimum_required(VERSION 3.10)
project(Aura)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Inclure les headers
include_directories(${CMAKE_SOURCE_DIR}/include)

# Ajouter l'exécutable
add_executable(aura src/main.cpp)

# Trouver OpenCV
find_package(OpenCV REQUIRED)
if(OpenCV_FOUND)
    include_directories(${OpenCV_INCLUDE_DIRS})
    target_link_libraries(aura ${OpenCV_LIBS})
else()
    message(FATAL_ERROR "OpenCV not found!")
endif()
