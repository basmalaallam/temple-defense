cmake_minimum_required(VERSION 3.10)
project(OpenGLStarter)

add_subdirectory(external/glfw-3.5.1)
add_subdirectory(external/glm)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

add_executable(OpenGLStarter
    main.cpp
    audio.cpp
)

target_include_directories(OpenGLStarter PRIVATE external/glad/include external/miniaudio)
target_link_libraries(OpenGLStarter PRIVATE glfw OpenGL::GL glm::glm Threads::Threads ${CMAKE_DL_LIBS})
if(UNIX AND NOT APPLE)
    target_link_libraries(OpenGLStarter PRIVATE m)
endif()

# Copy the sound files next to the executable so assets/sounds/... resolves.
add_custom_command(TARGET OpenGLStarter POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:OpenGLStarter>/assets
)
