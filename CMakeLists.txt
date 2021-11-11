cmake_minimum_required(VERSION 3.14.5)

# Varibales setting
set(MY_PROJECT "VolumeRendering")
set(MY_LIBRARY "Nexus")

# Enable C++ 17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project (${MY_PROJECT} LANGUAGES CXX C)

add_subdirectory(External/Nexus)

# Excecutable file setting
add_executable(${MY_PROJECT} Source/Main.cpp)
target_link_libraries(${MY_PROJECT} PUBLIC ${MY_LIBRARY})

# Copy these shader files
add_custom_command(TARGET ${MY_PROJECT} POST_BUILD COMMAND ${CMAKE_COMMAND} -E create_symlink
	${CMAKE_SOURCE_DIR}/Shaders/ ${CMAKE_BINARY_DIR}/Shaders/)
add_custom_command(TARGET ${MY_PROJECT} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory
	${CMAKE_SOURCE_DIR}/Resource/ ${CMAKE_BINARY_DIR}/Resource/)