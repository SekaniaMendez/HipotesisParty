add_library(GameCore STATIC game.cpp)

target_include_directories(GameCore
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)