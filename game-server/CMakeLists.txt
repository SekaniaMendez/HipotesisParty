add_executable(SessionWorker SessionWorker.cpp)
target_include_directories(SessionWorker PRIVATE ${PROJECT_SOURCE_DIR}/third_party)
target_link_libraries(SessionWorker PRIVATE GameCore)
