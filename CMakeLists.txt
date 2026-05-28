cmake_minimum_required(VERSION 3.16)
project(Arknight)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Disable SDL2_image heavy dependencies to prevent git fetch hangs
set(SDL2IMAGE_WEBP OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_AVIF OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_TIF OFF CACHE BOOL "" FORCE)

# Use system freetype instead of vendored (empty git submodule)
set(SDL2TTF_VENDORED OFF CACHE BOOL "" FORCE)

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
    src/Ark/AnimationLoader.cpp
    src/Ark/Camera.cpp
    src/Ark/EnemySystem.cpp
    src/Ark/OperatorSystem.cpp
    src/Ark/Renderer/Scene.cpp
    src/Ark/Renderer/Grid.cpp
    src/Ark/Renderer/Entities.cpp
    src/Ark/Renderer/Deploy.cpp
    src/Ark/Renderer/Hud.cpp
    src/Ark/GameLogic.cpp
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
