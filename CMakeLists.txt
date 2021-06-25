cmake_minimum_required (VERSION 3.6)
set(CMAKE_GENERATOR_PLATFORM x64)
set(CMAKE_BUILD_TYPE DEBUG|RELEASE)

project(SphereRendering)

set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR})

include_directories(${CMAKE_SOURCE_DIR}/include)

set(srcs ${CMAKE_SOURCE_DIR}/src/glad.c ${CMAKE_SOURCE_DIR}/src/Source.cpp ${CMAKE_SOURCE_DIR}/src/Source1.cpp)


add_executable(SphereRendering ${srcs})
target_link_libraries(SphereRendering ${CMAKE_SOURCE_DIR}/lib/glfw/glfw3.lib opengl32.lib)

install(TARGETS SphereRendering DESTINATION bin)
install(DIRECTORY Skybox DESTINATION bin)

