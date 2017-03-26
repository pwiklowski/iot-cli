project(iot-cli)
cmake_minimum_required(VERSION 2.8)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Werror=sign-compare")

FIND_LIBRARY(COAP PATHS ../libcoap/ NAMES coap)
FIND_LIBRARY(OIC PATHS ../liboic/ NAMES oic)

INCLUDE_DIRECTORIES(../liboic/)
INCLUDE_DIRECTORIES(../libcoap/)
INCLUDE_DIRECTORIES(../std/)

aux_source_directory(. SRC_LIST)

add_executable(${PROJECT_NAME} ${SRC_LIST})

target_link_libraries(${PROJECT_NAME} ${COAP})
target_link_libraries(${PROJECT_NAME} ${OIC})
target_link_libraries(${PROJECT_NAME}  pthread)
