set(kio_gdrive_SRCS
    kio_gdrive.cpp
    pathcache.cpp
    accountmanager.cpp
    gdrivehelper.cpp
)

ecm_qt_declare_logging_category(kio_gdrive_SRCS
    HEADER gdrivedebug.h
    IDENTIFIER GDRIVE
    CATEGORY_NAME kf5.kio.gdrive)


add_library(kio_gdrive MODULE ${kio_gdrive_SRCS})

target_link_libraries(kio_gdrive
    Qt5::Core
    Qt5::Network
    KF5::GAPICore
    KF5::GAPIDrive
    KF5::KIOCore
    KF5::KIOWidgets
    KF5::I18n
    KF5::Wallet)

set_target_properties(kio_gdrive PROPERTIES OUTPUT_NAME "gdrive")

install(TARGETS kio_gdrive DESTINATION ${PLUGIN_INSTALL_DIR}/kf5/kio)
