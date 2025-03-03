cmake_minimum_required(VERSION 3.10)

# Set C 23 standard
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED True)

# Set the project name and language
project(cli C)
include_directories(include)

# Optimize the build
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -s -fdata-sections -ffunction-sections -fno-exceptions")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s -fdata-sections -ffunction-sections")
endif()

add_compile_definitions(
  "FMT_HEADER_ONLY"
)

# Add the executable
add_executable(cli src/main.c src/sock_serv.c)

add_compile_definitions(MILLENNIUM_VERSION="${MILLENNIUM_VERSION}")

set_target_properties(cli PROPERTIES OUTPUT_NAME "millennium")
set_target_properties(cli PROPERTIES PREFIX "")

install(TARGETS cli DESTINATION bin)
