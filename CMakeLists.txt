cmake_minimum_required(VERSION 3.10)
project(bricked_up)

if(WIN32)
    # Windows-specific configuration
    set(SDL3_PATH "C:/SDL3")
    set(SDL3_TTF_PATH "C:/SDL3_ttf")
    set(SDL3_IMAGE_PATH "C:/SDL3_image")

    if(NOT EXISTS ${SDL3_PATH} OR NOT EXISTS ${SDL3_TTF_PATH} OR NOT EXISTS ${SDL3_IMAGE_PATH})
        message(FATAL_ERROR "SDL3, SDL3_ttf or SDL3_image not found. Please install them in C:/")
    endif()

    include_directories(${SDL3_PATH}/include ${SDL3_TTF_PATH}/include ${SDL3_IMAGE_PATH}/include)
    link_directories(${SDL3_PATH}/lib ${SDL3_TTF_PATH}/lib ${SDL3_IMAGE_PATH}/lib)
    set(LIBRARIES SDL3 SDL3_ttf SDL3_image)
else()
    # Linux-specific configuration
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL3 REQUIRED sdl3)
    pkg_check_modules(SDL3_TTF REQUIRED sdl3-ttf)
    pkg_check_modules(SDL3_IMAGE REQUIRED sdl3-image)
    include_directories(${SDL3_INCLUDE_DIRS} ${SDL3_TTF_INCLUDE_DIRS} ${SDL3_IMAGE_INCLUDE_DIRS})
    set(LIBRARIES ${SDL3_LIBRARIES} ${SDL3_TTF_LIBRARIES} ${SDL3_IMAGE_LIBRARIES})
endif()

add_executable(bricked_up src/main.c)

add_custom_command(TARGET bricked_up POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:bricked_up>/assets
)

target_link_libraries(bricked_up PRIVATE ${LIBRARIES} m)
