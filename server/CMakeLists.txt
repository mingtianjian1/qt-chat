add_executable(chatserver main.cpp chatserver.h chatserver.cpp clientsession.h clientsession.cpp)
target_link_libraries(chatserver PRIVATE chat_common Qt6::Network)
