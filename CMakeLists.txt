cmake_minimum_required(VERSION 2.8)
project(Estray)

set(BOOST_ROOT /opt/boost)
find_package(
    Boost
    1.75 REQUIRED
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
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -g -DBINARYARCHIVE -pthread")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -g -DXMLARCHIVE -pthread")

set(EXEC_SOURCE_FILES
    main.cpp
    payload.cpp
    misc.cpp)

add_executable(Estray ${EXEC_SOURCE_FILES})

TARGET_LINK_LIBRARIES(
    Estray
    ${Boost_LIBRARIES}
)
