kcoreaddons_add_plugin(gdrivecontextmenuaction
    SOURCES contextmenuaction.cpp
    INSTALL_NAMESPACE "kf6/kfileitemaction")

target_link_libraries(gdrivecontextmenuaction
    KF6::I18n
    KF6::KIOWidgets)
