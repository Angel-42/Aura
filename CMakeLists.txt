cmake_minimum_required(VERSION 3.16)
project(Aura LANGUAGES CXX)

# ---- Standard ----
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Options ----
option(BUILD_TESTS "Build unit tests" OFF)
option(BUILD_DEMO  "Build SFML demo application" OFF)

# SFML via Homebrew sur macOS
if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew")
endif()

# ---- Dépendances ----
find_package(OpenCV REQUIRED)

# ---- Sources partagées (sans main.cpp) ----
set(AURA_LIB_SOURCES
    src/Core/KalmanFilter.cpp
    src/Config/CalibConfig.cpp
    src/Config/ProfileManager.cpp
    src/Vision/Camera.cpp
    src/Vision/Calibrator.cpp
    src/Vision/HandTracker.cpp
    src/Vision/GestureDetector.cpp
    src/Input/Controller.cpp
    src/Input/Mapper.cpp
    src/App/AuraRunner.cpp
)

# ---- Bibliothèque statique partagée (tests + binaire) ----
add_library(aura_lib STATIC ${AURA_LIB_SOURCES})
target_include_directories(aura_lib PUBLIC include ${OpenCV_INCLUDE_DIRS})

if(TARGET OpenCV::OpenCV)
    target_link_libraries(aura_lib PUBLIC OpenCV::OpenCV)
else()
    target_link_libraries(aura_lib PUBLIC ${OpenCV_LIBS})
endif()

if(APPLE)
    target_link_libraries(aura_lib PUBLIC "-framework ApplicationServices")
elseif(UNIX)
    find_library(X11_LIB X11 REQUIRED)
    find_library(XTEST_LIB XTest REQUIRED)
    target_link_libraries(aura_lib PUBLIC ${X11_LIB} ${XTEST_LIB})
endif()

# ---- Binaire principal ----
add_executable(aura src/main.cpp)
target_link_libraries(aura PRIVATE aura_lib)

# ---- Config : copier le mapping dans le répertoire de build ----
add_custom_command(TARGET aura POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:aura>/config"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/config/default_mapping.txt"
            "$<TARGET_FILE_DIR:aura>/config/default_mapping.txt"
    COMMENT "Copie du mapping par défaut dans le répertoire de build"
)

# ---- Tests ----
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---- Demo SFML ----
if(BUILD_DEMO)
    add_subdirectory(demo)
endif()
