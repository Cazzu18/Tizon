cmake_minimum_required(VERSION 4.3.5)

project(Tizon)

add_executable(Tizon src/app/main.cpp src/engine/platform/window/Window.cpp)
target_compile_features(Tizon PRIVATE cxx_std_20) #Tizon needs C++ 20

find_package (glfw3 CONFIG REQUIRED)
find_package (Vulkan 1.4.335 REQUIRED)      # Require Vulkan SDK version 1.4.335 or higher
target_include_directories(Tizon PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(Tizon PRIVATE glfw Vulkan::Vulkan)
