# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(hands_on_lab1)

target_sources(app PRIVATE src/main.c thread_1.c)

target_include_directories(app PRIVATE include)