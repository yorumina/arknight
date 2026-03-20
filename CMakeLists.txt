cmake_minimum_required(VERSION 3.16)
project(Arknight)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add the PTSD submodule
add_subdirectory(PTSD)

# Ensure executable sees the correct dependencies and includes.
find_package(SDL2 REQUIRED)

set(ARKNIGHT_SRC
    src/App.cpp
    src/Cat.cpp
    src/Giraffe.cpp
    src/GiraffeText.cpp
    src/main.cpp
    src/Ark/StageLoader.cpp
)

add_executable(Arknight
    ${ARKNIGHT_SRC}
)

# The same include paths that 'Example' had inside PTSD
target_include_directories(Arknight SYSTEM PRIVATE
    ${DEPENDENCY_INCLUDE_DIRS}
    PTSD/include
)
target_include_directories(Arknight PRIVATE
    include
    include/Ark
)

# Link PTSD and other required libraries
target_link_libraries(Arknight
    PTSD
    SDL2::SDL2main
)

target_compile_definitions(Arknight PRIVATE ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/PTSD/assets")

# Re-link ArknightBuilder from the main CMakeLists.txt instead of PTSD/CMakeLists.txt
file(GLOB ARKNIGHT_BUILDER_SRC "tools/ark_builder/src/*.cpp")
add_executable(ArknightBuilder
    ${ARKNIGHT_BUILDER_SRC}
)
target_link_libraries(ArknightBuilder
    nlohmann_json
)
target_include_directories(ArknightBuilder SYSTEM PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/PTSD/third_party/nlohmann_json/include
)
target_compile_options(ArknightBuilder PRIVATE
    -Wall -Wextra -pedantic
)
