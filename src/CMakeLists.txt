include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_BINARY_DIR})

set(kio_gdrive_SRCS
    kio_gdrive.cpp
    pathcache.cpp
    abstractaccountmanager.cpp
    accountmanager.cpp
    kaccountsmanager.cpp
    gdrivehelper.cpp
    gdriveurl.cpp)

ecm_qt_declare_logging_category(kio_gdrive_SRCS
    HEADER gdrivedebug.h
    IDENTIFIER GDRIVE
    CATEGORY_NAME kf5.kio.gdrive)

kcoreaddons_add_plugin(kio_gdrive
    SOURCES ${kio_gdrive_SRCS}
    INSTALL_NAMESPACE kf5/kio)

target_link_libraries(kio_gdrive
    Qt5::Core
    Qt5::Network
    KAccounts
    ${QTKEYCHAIN_LIBRARIES}
    KF5::GAPICore
    KF5::GAPIDrive
    KF5::KIOCore
    KF5::KIOWidgets
    KF5::I18n)

set_target_properties(kio_gdrive PROPERTIES OUTPUT_NAME "gdrive")
