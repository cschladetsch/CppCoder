add_executable(replay_demo replay_demo.cpp)
target_link_libraries(replay_demo PRIVATE nlohmann_json::nlohmann_json Threads::Threads)

add_executable(minimal_usage minimal_usage.cpp)
target_link_libraries(minimal_usage PRIVATE cppcoder_core)
