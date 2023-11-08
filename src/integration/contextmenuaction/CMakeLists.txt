kcoreaddons_add_plugin(gdrivecontextmenuaction
    SOURCES contextmenuaction.cpp
    INSTALL_NAMESPACE "kf${QT_MAJOR_VERSION}/kfileitemaction")

target_link_libraries(gdrivecontextmenuaction
    KF${QT_MAJOR_VERSION}::I18n
    KF${QT_MAJOR_VERSION}::KIOWidgets)
