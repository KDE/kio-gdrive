kcoreaddons_add_plugin(gdrivecopyurlitemaction
    SOURCES copyurlitemaction.cpp
    JSON copyurlitemaction.json
    INSTALL_NAMESPACE "kf5/kfileitemaction")

target_link_libraries(gdrivecopyurlitemaction
    KF5::I18n
    KF5::KIOWidgets)
