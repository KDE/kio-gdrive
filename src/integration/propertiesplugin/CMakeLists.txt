set(gdrivepropertiesplugin_SRCS
    gdrivepropertiesplugin.cpp
    ../../gdrivedebug.cpp
)

ki18n_wrap_ui(gdrivepropertiesplugin_SRCS gdrivepropertiesplugin.ui)

kcoreaddons_add_plugin(gdrivepropertiesplugin
    SOURCES ${gdrivepropertiesplugin_SRCS}
    JSON gdrivepropertiesplugin.json
    INSTALL_NAMESPACE "kf${QT_MAJOR_VERSION}/propertiesdialog")

target_link_libraries(gdrivepropertiesplugin
    KF5::I18n
    KF5::KIOWidgets
)
