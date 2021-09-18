# Define ruler library target

add_library(crwruler STATIC)
target_sources(crwruler
        PRIVATE crw-ruler.h
        PRIVATE crw-ruler.c)
target_link_libraries(crwruler
        PRIVATE PkgConfig::GTK)

install(TARGETS crwruler DESTINATION libs)
install(FILES crw-ruler.h DESTINATION include)