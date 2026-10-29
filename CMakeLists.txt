cmake_minimum_required(VERSION 3.24)

project(AudioRoleSync VERSION 0.0.1 LANGUAGES CXX RC)

configure_file(
    resources/version.rc.in
    ${CMAKE_CURRENT_BINARY_DIR}/version.rc
    @ONLY
)

add_executable(AudioRoleSync WIN32
    src/main.cpp
    resources/app.rc
    ${CMAKE_CURRENT_BINARY_DIR}/version.rc
)

target_include_directories(AudioRoleSync PRIVATE resources)

target_compile_features(AudioRoleSync PRIVATE cxx_std_20)
target_compile_definitions(AudioRoleSync PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

target_link_libraries(AudioRoleSync PRIVATE
    ole32
    shell32
    user32
    advapi32
    uuid
)

if(MSVC)
    set_property(TARGET AudioRoleSync PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )

    target_compile_options(AudioRoleSync PRIVATE
        $<$<CONFIG:Release>:/O1 /GL /Gy>
    )

    target_link_options(AudioRoleSync PRIVATE
        $<$<CONFIG:Release>:/LTCG /OPT:REF /OPT:ICF>
    )
endif()
