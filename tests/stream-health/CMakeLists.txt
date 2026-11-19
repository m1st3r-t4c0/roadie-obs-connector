cmake_minimum_required(VERSION 3.28)
project(stream_health_probe LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
add_executable(stream-health-probe probe.cpp ../../src/encoder-frames.hpp)
