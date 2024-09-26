find_package(KF6Notifications ${KF_MIN_VERSION} REQUIRED)
find_package(Intltool REQUIRED)

kaccounts_add_service(${CMAKE_CURRENT_SOURCE_DIR}/google-drive.service.in)

kcoreaddons_add_plugin(kaccounts_gdrive
    SOURCES kaccountsplugin.cpp
    INSTALL_NAMESPACE kaccounts/daemonplugins)

set_target_properties(kaccounts_gdrive PROPERTIES OUTPUT_NAME "gdrive")
target_link_libraries(kaccounts_gdrive
    KAccounts6
    KF6::KIOWidgets
    KF6::I18n
    KF6::Notifications)

install(FILES gdrive.notifyrc DESTINATION ${KDE_INSTALL_KNOTIFYRCDIR})
