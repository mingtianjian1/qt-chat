add_library(chat_common STATIC protocol.h protocol.cpp)
target_include_directories(chat_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(chat_common PUBLIC Qt6::Core)
