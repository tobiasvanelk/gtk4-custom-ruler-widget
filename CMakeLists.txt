# Define project properties

cmake_minimum_required(VERSION 3.20)
project(gtk4-custom-ruler-widget
        VERSION 1.0.0
        LANGUAGES C)

set(CMAKE_C_STANDARD 99)

# Find GTK libraries

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK
        REQUIRED
        IMPORTED_TARGET
        gtk4)

if (GTK_FOUND)
    set_target_properties(PkgConfig::GTK PROPERTIES IMPORTED_GLOBAL TRUE)
endif()

add_subdirectory(ruler)
add_subdirectory(demo-app)