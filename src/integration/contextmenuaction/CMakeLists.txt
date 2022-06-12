kcoreaddons_add_plugin(gdrivecontextmenuaction
    SOURCES contextmenuaction.cpp
    JSON contextmenuaction.json
    INSTALL_NAMESPACE "kf${QT_MAJOR_VERSION}/kfileitemaction")

target_link_libraries(gdrivecontextmenuaction
    KF5::I18n
    KF5::KIOWidgets)
