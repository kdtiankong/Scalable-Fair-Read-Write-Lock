cmake_minimum_required(VERSION 3.16)
project(Scalable_Fair_Reader_Writer_Lock)

set(CMAKE_CXX_STANDARD 14)

add_executable(Scalable_Fair_Reader_Writer_Lock main.cpp ReaderWriterLock.h SNZI.h)