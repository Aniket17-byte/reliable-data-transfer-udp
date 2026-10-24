cmake_minimum_required(VERSION 3.16)
project(ReliableDataTransferUDP CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

# Platform-specific socket libraries
if(WIN32)
    set(EXTRA_LIBS ws2_32)
else()
    set(EXTRA_LIBS pthread)
endif()

# 1. Channel Emulator
add_executable(channel_emulator
    src/channel_emulator.cpp
    src/main_emulator.cpp
)
target_link_libraries(channel_emulator PRIVATE ${EXTRA_LIBS})

# 2. Stop-and-Wait Sender & Receiver
add_executable(sender_sw src/sender_sw.cpp)
target_link_libraries(sender_sw PRIVATE ${EXTRA_LIBS})

add_executable(receiver_sw src/receiver_sw.cpp)
target_link_libraries(receiver_sw PRIVATE ${EXTRA_LIBS})

# 3. Go-Back-N Sender & Receiver
add_executable(sender_gbn src/sender_gbn.cpp)
target_link_libraries(sender_gbn PRIVATE ${EXTRA_LIBS})

add_executable(receiver_gbn src/receiver_gbn.cpp)
target_link_libraries(receiver_gbn PRIVATE ${EXTRA_LIBS})

# 4. Selective Repeat Sender & Receiver
add_executable(sender_sr src/sender_sr.cpp)
target_link_libraries(sender_sr PRIVATE ${EXTRA_LIBS})

add_executable(receiver_sr src/receiver_sr.cpp)
target_link_libraries(receiver_sr PRIVATE ${EXTRA_LIBS})

# 5. Emulator Unit Tests
add_executable(test_emulator
    src/channel_emulator.cpp
    tests/test_emulator.cpp
)
target_link_libraries(test_emulator PRIVATE ${EXTRA_LIBS})
