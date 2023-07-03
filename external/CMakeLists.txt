find_package(PNG REQUIRED)

add_library(png++ INTERFACE)
target_compile_features(png++ INTERFACE cxx_std_11)
target_include_directories(png++ INTERFACE "." ${PNG_INCLUDE_DIR})
target_link_libraries(png++ INTERFACE project_config ${PNG_LIBRARY})

