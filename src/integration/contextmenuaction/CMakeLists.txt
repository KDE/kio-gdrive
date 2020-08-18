kcoreaddons_add_plugin(gdrivecontextmenuaction
    SOURCES contextmenuaction.cpp
    JSON contextmenuaction.json
    INSTALL_NAMESPACE "kf5/kfileitemaction")

target_link_libraries(gdrivecontextmenuaction
    KF5::I18n
    KF5::KIOWidgets)
