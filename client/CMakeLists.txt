add_executable(chatclient main.cpp)
target_link_libraries(chatclient PRIVATE chat_common Qt6::Widgets Qt6::Network)
