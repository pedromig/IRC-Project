cmake_minimum_required(VERSION 3.15)
project(IRC_Project)

set(CMAKE_CXX_STANDARD 14)

add_executable(IRC_Project
        README.md Server/server.c Client/client.c Client/client.c Proxy/ircproxy.c Server/server.h Proxy/ircproxy.h Functions/functions.c Functions/functions.h Client/client.h)
