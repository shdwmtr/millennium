cmake_minimum_required(VERSION 3.10)

# Set the project name
project(ShimDll)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if (WIN32 AND NOT GITHUB_ACTION_BUILD)
  # debug output paths
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "C:/Program Files (x86)/Steam")
  set(LIBRARY_OUTPUT_DIRECTORY "C:/Program Files (x86)/Steam")
elseif(UNIX AND NOT GITHUB_ACTION_BUILD)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "$ENV{HOME}/.millennium/")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "$ENV{HOME}/.millennium/")
endif()

# Add the executable
add_library(ShimDll SHARED main.cc)

set_target_properties(ShimDll PROPERTIES OUTPUT_NAME "user32")
set_target_properties(ShimDll PROPERTIES PREFIX "")
set_target_properties(ShimDll PROPERTIES NO_EXPORT TRUE)

