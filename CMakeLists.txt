cmake_minimum_required(VERSION 4.0)

project(HypotesisParty VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# agregar ejecutable principal
add_executable(HypotesisParty main.cpp)

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

# enlazar bibliotecas
target_link_libraries(HypotesisParty PRIVATE
    GameCore
    GamePlatform
    raylib
)

# configuración para navegadores web
if(EMSCRIPTEN)
    set_target_properties(HypotesisParty PROPERTIES SUFFIX
        ".html"
    )
    target_link_options(HypotesisParty PRIVATE
        "-sUSE_GLFW=3"

        "-sALLOW_MEMORY_GROWTH=1"
    )

    # agregar objetivo personalizado para ejecutar el servidor web
    add_custom_target(correr-web
        COMMAND python3 -m http.server 8000
        --directory ${CMAKE_BINARY_DIR}
        COMMENT "Iniciando servidor web..."
        USES_TERMINAL
    )
endif()
