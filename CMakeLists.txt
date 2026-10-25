cmake_minimum_required(VERSION 3.20)
project(HipotesisParty LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(BUILD_WEB "Build the raylib browser client" OFF)

if(BUILD_WEB)
  if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "Use emcmake for BUILD_WEB")
  endif()
  set(PLATFORM Web CACHE STRING "" FORCE)
  set(GRAPHICS GRAPHICS_API_OPENGL_ES3 CACHE STRING "" FORCE)
  set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
  add_subdirectory(.tools/raylib raylib)
  add_executable(game client/main.cpp)
  target_link_libraries(game PRIVATE raylib)
  target_compile_options(game PRIVATE -O2)
  set_target_properties(game PROPERTIES
    SUFFIX ".js"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/web/build")
  target_link_options(game PRIVATE
    -O2
    "-sUSE_GLFW=3" "-sMIN_WEBGL_VERSION=2" "-sMAX_WEBGL_VERSION=2"
    "-sALLOW_MEMORY_GROWTH=1" "-sMODULARIZE=1" "-sEXPORT_NAME=createHypotesisGame"
    "-sENVIRONMENT=web" "-sFILESYSTEM=1"
    "-sEXPORTED_FUNCTIONS=['_main','_hp_resize','_hp_map','_hp_tile','_hp_players_clear','_hp_player','_hp_focus','_hp_station','_hp_stations_clear']"
    --preload-file "${CMAKE_SOURCE_DIR}/web/assets@/assets")
else()
  add_executable(maze_export
    native/maze_export.cpp
    vendor/treasure_labyrinth/src/Node.cpp
    vendor/treasure_labyrinth/src/DeepSearch.cpp
    vendor/treasure_labyrinth/src/BreadthSearch.cpp)
  target_include_directories(maze_export PRIVATE vendor/treasure_labyrinth/include)
  # The imported headers rely on these standard includes transitively.
  target_precompile_headers(maze_export PRIVATE <algorithm> <functional> <stack> <queue>)
endif()
