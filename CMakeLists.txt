cmake_minimum_required(VERSION 2.8)
project(Evaluator)

set(BOOST_ROOT /opt/boost)
find_package(
    Boost
    1.66 REQUIRED
    COMPONENTS
    system
    program_options
    serialization
)

INCLUDE_DIRECTORIES(
    ${Boost_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

LINK_DIRECTORIES(
    ${Boost_LIBRARY_DIRS}
)

# You may use different encoding types for the payloads here: BINARYARCHIVE, XMLARCHIVE and TEXTARCHIVE,
# implemented via the typical Boost.Serialization archives. Use -DDEBUG to switch on additional diagnostic messages.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -O3 -DBINARYARCHIVE -pthread")

set(EXEC_SOURCE_FILES
    async_websocket_server.cpp
    main.cpp
    misc.cpp
    payload.cpp
)

add_executable(Evaluator ${EXEC_SOURCE_FILES})

TARGET_LINK_LIBRARIES(
    Evaluator
    ${Boost_LIBRARIES}
)
