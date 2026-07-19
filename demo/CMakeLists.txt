find_package(SFML 3 COMPONENTS Graphics Window System REQUIRED)

add_executable(aura_demo
    main.cpp
    DemoApp.cpp
)

target_link_libraries(aura_demo PRIVATE
    aura_lib
    SFML::Graphics
    SFML::Window
    SFML::System
)

target_include_directories(aura_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

# Copie le mapping dans le répertoire de build de la démo
add_custom_command(TARGET aura_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:aura_demo>/config"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/config/default_mapping.txt"
            "$<TARGET_FILE_DIR:aura_demo>/config/default_mapping.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/config/browser.txt"
            "$<TARGET_FILE_DIR:aura_demo>/config/browser.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/config/gaming.txt"
            "$<TARGET_FILE_DIR:aura_demo>/config/gaming.txt"
    COMMENT "Copie des profils dans le répertoire de build de la démo"
)
