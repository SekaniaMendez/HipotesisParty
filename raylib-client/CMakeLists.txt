add_library(RaylibClient STATIC
    SpriteSheet.cpp
    Animation.cpp
    ResourceManager.cpp
    PartyUI.cpp
    SceneManager.cpp
    PartyGame.cpp
    SessionClient.cpp
)

target_include_directories(RaylibClient
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/third_party
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(RaylibClient PUBLIC raylib GameCore)
if(NOT EMSCRIPTEN)
    find_package(CURL REQUIRED)
    find_package(Threads REQUIRED)
    target_link_libraries(RaylibClient PUBLIC CURL::libcurl Threads::Threads)
    target_compile_definitions(RaylibClient PRIVATE HIPOTESIS_ASSET_DIR="${PROJECT_SOURCE_DIR}/assets")
endif()

if(BUILD_TESTING AND NOT EMSCRIPTEN)
    add_executable(RaylibClientAnimationTests tests/AnimationTests.cpp)
    target_link_libraries(RaylibClientAnimationTests PRIVATE RaylibClient)
    add_test(NAME RaylibClientAnimationTests COMMAND RaylibClientAnimationTests)

    add_executable(RaylibClientResourceManagerTests tests/ResourceManagerTests.cpp)
    target_link_libraries(RaylibClientResourceManagerTests PRIVATE RaylibClient)
    add_test(NAME RaylibClientResourceManagerTests COMMAND RaylibClientResourceManagerTests)
endif()

if(BUILD_TESTING AND NOT EMSCRIPTEN)
    add_executable(SceneManagerTests tests/SceneManagerTests.cpp)
    target_link_libraries(SceneManagerTests PRIVATE RaylibClient)
    add_test(NAME SceneManagerTests COMMAND SceneManagerTests)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/ModelAssetsTests.cpp")
        add_executable(ModelAssetsTests tests/ModelAssetsTests.cpp)
        target_link_libraries(ModelAssetsTests PRIVATE RaylibClient)
        target_compile_definitions(ModelAssetsTests PRIVATE HIPOTESIS_ASSET_DIR="${PROJECT_SOURCE_DIR}/assets")
    endif()
endif()
