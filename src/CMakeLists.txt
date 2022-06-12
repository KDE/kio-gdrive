include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_BINARY_DIR})

add_subdirectory(integration)

set(kio_gdrive_SRCS
    kio_gdrive.cpp
    pathcache.cpp
    abstractaccountmanager.cpp
    gdrivehelper.cpp
    gdriveurl.cpp)

if (KAccounts_FOUND)
    set(BACKEND_SRC kaccountsmanager.cpp)
    set(BACKEND_HEADER kaccountsmanager.h)
    set(BACKEND_CLASSNAME KAccountsManager)
    set(BACKEND_LIBS KAccounts)
else()
    set(BACKEND_SRC keychainaccountmanager.cpp)
    set(BACKEND_HEADER keychainaccountmanager.h)
    set(BACKEND_CLASSNAME KeychainAccountManager)
    set(BACKEND_LIBS ${QTKEYCHAIN_LIBRARIES})
endif()

set(kio_gdrive_SRCS ${kio_gdrive_SRCS} ${BACKEND_SRC})
configure_file(gdrivebackend.h.in ${CMAKE_CURRENT_BINARY_DIR}/gdrivebackend.h)

ecm_qt_declare_logging_category(kio_gdrive_SRCS
    HEADER gdrivedebug.h
    IDENTIFIER GDRIVE
    CATEGORY_NAME kf5.kio.gdrive)

kcoreaddons_add_plugin(kio_gdrive
    SOURCES ${kio_gdrive_SRCS}
    INSTALL_NAMESPACE kf${QT_MAJOR_VERSION}/kio)

target_link_libraries(kio_gdrive
    Qt::Core
    Qt::Network
    KPim::GAPICore
    KPim::GAPIDrive
    KF5::KIOCore
    KF5::KIOWidgets
    KF5::I18n
    ${BACKEND_LIBS})

set_target_properties(kio_gdrive PROPERTIES OUTPUT_NAME "gdrive")
