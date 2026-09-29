
include_directories(${PROJECT_SOURCE_DIR}/src)
add_executable(tests
    tests.cpp
    ${PROJECT_SOURCE_DIR}/src/slice_cache_policy.cpp
    ${PROJECT_SOURCE_DIR}/src/config.cpp
    ${PROJECT_SOURCE_DIR}/src/colormap_lut.cpp
)
target_link_libraries(tests TIFF::TIFF)
add_test(NAME tests COMMAND tests)
