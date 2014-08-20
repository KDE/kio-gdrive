set(kio_gdrive_SRCS
    kio_gdrive.cpp
    pathcache.cpp
    accountmanager.cpp
)

kde4_add_plugin(kio_gdrive ${kio_gdrive_SRCS})

target_link_libraries(kio_gdrive
    ${KDE4_KIO_LIBS}
    ${LibKGAPI2_LIBRARY}
)

install(TARGETS kio_gdrive DESTINATION ${PLUGIN_INSTALL_DIR})
