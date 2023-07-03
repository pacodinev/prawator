add_executable(main_app "main.cpp")
target_link_libraries(main_app PRIVATE execution_planner wator_gamecg project_config)

if(WATOR_BUILD_MAP_READER)
    add_executable(map_reader "map_reader.cpp")
    target_link_libraries(map_reader PRIVATE project_config)
    #target_link_libraries(map_reader PRIVATE -lturbojpeg)
    target_link_libraries(map_reader PRIVATE png++)
endif(WATOR_BUILD_MAP_READER)
