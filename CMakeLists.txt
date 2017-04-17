cmake_minimum_required(VERSION 3.6)
project(ReliableDataTransfer)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES ReliableDataSender.cpp)
add_executable(ReliableDataSender ${SOURCE_FILES})

set(SOURCE_FILES2 ReliableDataReceiver.cpp)
add_executable(ReliableDataReceiver ${SOURCE_FILES2})