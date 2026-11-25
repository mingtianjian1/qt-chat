add_executable(chatserver main.cpp)
target_link_libraries(chatserver PRIVATE chat_common Qt6::Network)
