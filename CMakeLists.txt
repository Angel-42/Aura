cmake_minimum_required(VERSION 3.16)
project(Aura LANGUAGES CXX)

# ---- Standard ----
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Dépendances ----
find_package(OpenCV REQUIRED)

# ---- Sources ----
set(AURA_SOURCES
    src/main.cpp

    # Core
    src/Core/KalmanFilter.cpp

    # Config
    src/Config/CalibConfig.cpp

    # Vision
    src/Vision/Camera.cpp
    src/Vision/Calibrator.cpp
    src/Vision/HandTracker.cpp
    src/Vision/GestureDetector.cpp

    # Input
    src/Input/Controller.cpp
    src/Input/Mapper.cpp

    # App
    src/App/AuraRunner.cpp
)

add_executable(aura ${AURA_SOURCES})

target_include_directories(aura PUBLIC include)

# ---- Link OpenCV ----
if(TARGET OpenCV::OpenCV)
    target_link_libraries(aura PRIVATE OpenCV::OpenCV)
else()
    target_link_libraries(aura PRIVATE ${OpenCV_LIBS})
endif()

# ---- Plateforme ----
if(APPLE)
    target_link_libraries(aura PRIVATE "-framework ApplicationServices")
elseif(UNIX)
    find_library(X11_LIB X11 REQUIRED)
    find_library(XTEST_LIB XTest REQUIRED)
    target_link_libraries(aura PRIVATE ${X11_LIB} ${XTEST_LIB})
endif()

# ---- Config : copier le fichier de mapping dans le répertoire de build ----
add_custom_command(TARGET aura POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:aura>/config"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/config/default_mapping.txt"
            "$<TARGET_FILE_DIR:aura>/config/default_mapping.txt"
    COMMENT "Copie du mapping par défaut dans le répertoire de build"
)
