# Small demo application to show ruler

add_executable(demo_app)
target_sources(demo_app
        PRIVATE main.c)
target_link_libraries(demo_app
        PRIVATE PkgConfig::GTK
        PRIVATE crwruler)
target_include_directories(demo_app
        PRIVATE ${CMAKE_SOURCE_DIR}/ruler)

# Copy required assets for the demo app
configure_file(builder.ui builder.ui COPYONLY)
configure_file(central-park-nyc.jpg central-park-nyc.jpg COPYONLY)