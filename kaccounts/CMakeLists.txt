find_package(KF5Notifications ${KF5_MIN_VERSION} REQUIRED)
find_package(Intltool REQUIRED)

kaccounts_add_service(${CMAKE_CURRENT_SOURCE_DIR}/google-drive.service.in)

kcoreaddons_add_plugin(kaccounts_gdrive
    JSON kaccountsplugin.json
    SOURCES kaccountsplugin.cpp
    INSTALL_NAMESPACE kaccounts/daemonplugins)

set_target_properties(kaccounts_gdrive PROPERTIES OUTPUT_NAME "gdrive")
target_link_libraries(kaccounts_gdrive
    KAccounts
    KF5::KIOWidgets
    KF5::I18n
    KF5::Notifications)

install(FILES gdrive.notifyrc DESTINATION ${KDE_INSTALL_KNOTIFY5RCDIR})
