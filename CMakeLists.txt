cmake_minimum_required(VERSION 4.0)

project(HypotesisParty VERSION 1.0.0 LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# agregar ejecutable principal
add_executable(HypotesisParty main.cpp core/game.cpp)

if(EMSCRIPTEN)
    # Compilar Raylib para WebAssembly
    include(FetchContent)

    set(PLATFORM Web CACHE STRING "" FORCE)

    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 6.2
    )

    FetchContent_MakeAvailable(raylib)
else()
    # Utilizar Raylib instalada mediante Homebrew o apt
    find_package(raylib CONFIG REQUIRED)
endif()

# agregar subdirectorios
add_subdirectory(core)
add_subdirectory(platform)
add_subdirectory(raylib-client)
if(NOT EMSCRIPTEN)
    add_subdirectory(game-server)
endif()

# enlazar bibliotecas
target_link_libraries(HypotesisParty PRIVATE
    GameCore
    GamePlatform
    raylib
    RaylibClient
)

# configuración para navegadores web
if(EMSCRIPTEN)
    set_target_properties(HypotesisParty PROPERTIES SUFFIX
        ".html"
    )
    foreach(CPP_TARGET HypotesisParty RaylibClient GameCore)
        target_compile_options(${CPP_TARGET} PRIVATE -fexceptions)
    endforeach()
    target_link_options(HypotesisParty PRIVATE
        "-fexceptions"
        "-sDISABLE_EXCEPTION_CATCHING=0"
        "-sUSE_GLFW=3"
        "SHELL:--shell-file \"${CMAKE_SOURCE_DIR}/web/shell.html\""

        "-sALLOW_MEMORY_GROWTH=1"
        "SHELL:--preload-file \"${CMAKE_SOURCE_DIR}/assets/models@assets/models\""
        "SHELL:--preload-file \"${CMAKE_SOURCE_DIR}/assets/shaders@assets/shaders\""
        "SHELL:--preload-file \"${CMAKE_SOURCE_DIR}/assets/fonts@assets/fonts\""
    )

    file(GLOB_RECURSE GAME_RUNTIME_ASSETS CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/assets/models/*"
        "${CMAKE_SOURCE_DIR}/assets/shaders/*"
        "${CMAKE_SOURCE_DIR}/assets/fonts/*")
    set_property(TARGET HypotesisParty APPEND PROPERTY LINK_DEPENDS ${GAME_RUNTIME_ASSETS} "${CMAKE_SOURCE_DIR}/web/shell.html")

    # agregar objetivo personalizado para ejecutar el servidor web
    add_custom_target(correr-web
        COMMAND python3 ${CMAKE_SOURCE_DIR}/game-server/server.py
        --web-dir ${CMAKE_BINARY_DIR} --worker ${CMAKE_SOURCE_DIR}/build/game-server/SessionWorker
        COMMENT "Iniciando servidor multijugador en 127.0.0.1:8001..."
        USES_TERMINAL
    )
endif()

# Runtime assets are portable next to the executable; Blender source stays out
# of the web payload. Re-run this target after regenerating models.
if(NOT EMSCRIPTEN)
    add_custom_target(copy-game-assets ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets/models ${CMAKE_BINARY_DIR}/assets/models
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets/shaders ${CMAKE_BINARY_DIR}/assets/shaders
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets/fonts ${CMAKE_BINARY_DIR}/assets/fonts)
    add_dependencies(HypotesisParty copy-game-assets)
endif()
