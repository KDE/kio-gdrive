find_package(KF${QT_MAJOR_VERSION}Notifications ${KF_MIN_VERSION} REQUIRED)
find_package(Intltool REQUIRED)

kaccounts_add_service(${CMAKE_CURRENT_SOURCE_DIR}/google-drive.service.in)

kcoreaddons_add_plugin(kaccounts_gdrive
    SOURCES kaccountsplugin.cpp
    INSTALL_NAMESPACE kaccounts/daemonplugins)

set_target_properties(kaccounts_gdrive PROPERTIES OUTPUT_NAME "gdrive")
target_link_libraries(kaccounts_gdrive
    KAccounts${KACCOUNTS_SUFFIX}
    KF${QT_MAJOR_VERSION}::KIOWidgets
    KF${QT_MAJOR_VERSION}::I18n
    KF${QT_MAJOR_VERSION}::Notifications)

install(FILES gdrive.notifyrc DESTINATION ${KDE_INSTALL_KNOTIFYRCDIR})
