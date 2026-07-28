cmake_minimum_required(VERSION 3.20)
project(AlphaPigGUI LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- SFML (prebuilt) ----
set(SFML_DIR "C:/SFML-2.6.2/lib/cmake/SFML")
find_package(SFML 2.6 COMPONENTS graphics window system REQUIRED)

# ---- Engine directory (for #include resolution in EngineBridge.cpp) ----
set(ENGINE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../SelfPlay")

# ---- Executable ----
add_executable(${PROJECT_NAME}
    main.cpp
    BoardRenderer.cpp
    GameController.cpp
    EngineBridge.cpp
    BoardRenderer.h
    GameController.h
    EngineBridge.h
    Engine.h
    SearchInfo.h
)

# Include paths for engine and local headers
target_include_directories(${PROJECT_NAME} PRIVATE
    ${ENGINE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# UTF-8 encoding for MSVC (engine files contain Chinese comments)
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif()

# ---- Intel MKL ----
set(MKL_ROOT "C:/Program Files (x86)/Intel/oneAPI/mkl/latest")
if(EXISTS "${MKL_ROOT}")
    target_include_directories(${PROJECT_NAME} PRIVATE "${MKL_ROOT}/include")
    target_link_directories(${PROJECT_NAME} PRIVATE "${MKL_ROOT}/lib")
    target_link_libraries(${PROJECT_NAME} PRIVATE
        mkl_intel_lp64 mkl_sequential mkl_core
    )
endif()

# ---- OpenMP ----
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE OpenMP::OpenMP_CXX)
endif()

# ---- SFML ----
target_link_libraries(${PROJECT_NAME} PRIVATE
    sfml-graphics sfml-window sfml-system
)

# Copy SFML DLLs to output dir
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "C:/SFML-2.6.2/bin/sfml-graphics-2.dll"
        "C:/SFML-2.6.2/bin/sfml-window-2.dll"
        "C:/SFML-2.6.2/bin/sfml-system-2.dll"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
)

# ---- SelfPlayValidate (M1.0 validation harness) ----
add_executable(SelfPlayValidate
    ../SelfPlay/SelfPlayValidate.cpp
)
target_include_directories(SelfPlayValidate PRIVATE
    ../SelfPlay
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}
    "${MKL_ROOT}/include"
)
target_link_directories(SelfPlayValidate PRIVATE "${MKL_ROOT}/lib")
if(MSVC)
    target_compile_options(SelfPlayValidate PRIVATE /utf-8 /openmp)
endif()
target_link_libraries(SelfPlayValidate PRIVATE
    mkl_intel_lp64 mkl_sequential mkl_core
)
if(OpenMP_CXX_FOUND)
    target_link_libraries(SelfPlayValidate PRIVATE OpenMP::OpenMP_CXX)
endif()

message(STATUS "AlphaPig GUI + SelfPlayValidate configured successfully")
