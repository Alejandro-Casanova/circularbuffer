cmake_minimum_required(VERSION 3.13)

cmake_policy(SET CMP0079 NEW)

project("circularbuffer")

add_library(${PROJECT_NAME} INTERFACE)

target_sources(${PROJECT_NAME}
  INTERFACE 
    FILE_SET HEADERS
)

target_compile_features(${PROJECT_NAME}
  INTERFACE
    cxx_std_20
)

add_subdirectory(samples)

enable_testing()
add_subdirectory(tests)

install(
  TARGETS circularbuffer
  DESTINATION lib
)
