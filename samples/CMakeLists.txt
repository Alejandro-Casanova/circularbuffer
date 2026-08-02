add_executable(sample_int_buffer sample_int_buffer.cpp)
target_link_libraries(sample_int_buffer
	PRIVATE
		circularbuffer
)

add_executable(sample_string_buffer sample_string_buffer.cpp)
target_link_libraries(sample_string_buffer
	PRIVATE
		circularbuffer
)

add_executable(sample_custom_buffer sample_custom_buffer.cpp)
target_link_libraries(sample_custom_buffer
	PRIVATE
		circularbuffer
)

if(
  (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC") OR
  (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
)
	message(NOTICE "Compiling with MSVC")
  target_compile_options(sample_int_buffer PRIVATE /W4)
	target_compile_options(sample_string_buffer PRIVATE /W4)
	target_compile_options(sample_custom_buffer PRIVATE /W4)
elseif(
  (CMAKE_CXX_COMPILER_ID STREQUAL "GNU") OR
  (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
)
	message(NOTICE "Compiling with GNU/Clang")
  target_compile_options(sample_int_buffer PRIVATE -Wall -Wextra -Wpedantic)
	target_compile_options(sample_string_buffer PRIVATE -Wall -Wextra -Wpedantic)
	target_compile_options(sample_custom_buffer PRIVATE -Wall -Wextra -Wpedantic)
else()
	message(WARNING "Unknown compiler: ${CMAKE_CXX_COMPILER_ID}")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	message(NOTICE "Compiling with Clang (specifically)")
  target_compile_options(sample_int_buffer PRIVATE -Weverything)
	target_compile_options(sample_string_buffer PRIVATE -Weverything)
	target_compile_options(sample_custom_buffer PRIVATE -Weverything)
endif()
