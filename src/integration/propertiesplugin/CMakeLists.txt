set(gdrivepropertiesplugin_SRCS
    gdrivepropertiesplugin.cpp
    ../../gdrivedebug.cpp
)

ki18n_wrap_ui(gdrivepropertiesplugin_SRCS gdrivepropertiesplugin.ui)

kcoreaddons_add_plugin(gdrivepropertiesplugin
    SOURCES ${gdrivepropertiesplugin_SRCS}
    INSTALL_NAMESPACE "kf${QT_MAJOR_VERSION}/propertiesdialog")

target_link_libraries(gdrivepropertiesplugin
    KF${QT_MAJOR_VERSION}::I18n
    KF${QT_MAJOR_VERSION}::KIOWidgets
)
