set(kio_gdrive_SRCS
    kio_gdrive.cpp
    pathcache.cpp
    accountmanager.cpp
    gdrivehelper.cpp
)

kde4_add_plugin(kio_gdrive ${kio_gdrive_SRCS})

target_link_libraries(kio_gdrive
    Qt5::Core
    Qt5::Network
    KF5::GAPICore
    KF5::GAPIDrive
    KF5::KIOCore
    KF5::KIOWidgets
    KF5::I18n
    KF5::Wallet)

install(TARGETS kio_gdrive DESTINATION ${PLUGIN_INSTALL_DIR}/kf5/kio)
