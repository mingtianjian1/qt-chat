add_executable(chatclient
    main.cpp
    mainwindow.h
    mainwindow.cpp
    chatclient.h
    chatclient.cpp
    chattypes.h
    worker.h
    worker.cpp
    historystore.h
    historystore.cpp
)
target_link_libraries(chatclient PRIVATE chat_common Qt6::Widgets Qt6::Network Qt6::Sql)
