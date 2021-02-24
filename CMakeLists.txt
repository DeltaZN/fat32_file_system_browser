cmake_minimum_required(VERSION 3.15)
project(lab1 C)

set(CMAKE_C_STANDARD 11)

add_executable(lab1 main.c utils.h shell_mode.h list_mode.h fat32_lib.h)