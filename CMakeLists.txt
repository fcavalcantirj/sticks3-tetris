cmake_minimum_required(VERSION 3.16)
project(stackfall CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(STRINGS "${CMAKE_SOURCE_DIR}/VERSION" SF_VERSION LIMIT_COUNT 1)
add_compile_definitions(SF_VERSION="${SF_VERSION}")

include_directories(src test/host include)

file(GLOB_RECURSE SF_CORE_SOURCES CONFIGURE_DEPENDS src/stackfall/*.cpp)
file(GLOB SF_TEST_SOURCES CONFIGURE_DEPENDS test/host/*.cpp)
list(REMOVE_ITEM SF_TEST_SOURCES ${CMAKE_SOURCE_DIR}/test/host/sim_main.cpp)
list(REMOVE_ITEM SF_TEST_SOURCES ${CMAKE_SOURCE_DIR}/test/host/replay_main.cpp)

add_executable(sf_tests ${SF_CORE_SOURCES} ${SF_TEST_SOURCES})
target_compile_options(sf_tests PRIVATE -Wall -Wextra -Werror -fsanitize=address,undefined
                                        -fno-omit-frame-pointer)
target_link_options(sf_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)

add_executable(sf_sim ${SF_CORE_SOURCES} test/host/sim_main.cpp)
target_compile_options(sf_sim PRIVATE -Wall -Wextra -Werror -fsanitize=address,undefined
                                      -fno-omit-frame-pointer)
target_link_options(sf_sim PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)

add_executable(sf_replay ${SF_CORE_SOURCES} test/host/replay_main.cpp)
target_compile_options(sf_replay PRIVATE -Wall -Wextra -Werror -fsanitize=address,undefined
                                         -fno-omit-frame-pointer)
target_link_options(sf_replay PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)

enable_testing()
add_test(NAME sf_tests COMMAND sf_tests)
