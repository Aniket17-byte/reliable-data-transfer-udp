cmake_minimum_required(VERSION 3.15)
project(ReliableDataTransferUDP CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

# Core static library for Aniket's modules
add_library(rdt_core STATIC
    src/rtt_estimator.cpp
    src/timer_manager.cpp
    src/gbn_window.cpp
    src/sr_window.cpp
)

# Test executable
add_executable(run_tests
    tests/test_main.cpp
    tests/test_rtt.cpp
    tests/test_timers.cpp
    tests/test_gbn.cpp
    tests/test_sr.cpp
)

target_link_libraries(run_tests PRIVATE rdt_core)
