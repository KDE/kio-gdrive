set(gdrivepropertiesplugin_SRCS
    gdrivepropertiesplugin.cpp
    ../../gdrivedebug.cpp
)

ki18n_wrap_ui(gdrivepropertiesplugin_SRCS gdrivepropertiesplugin.ui)

kcoreaddons_add_plugin(gdrivepropertiesplugin
    SOURCES ${gdrivepropertiesplugin_SRCS}
    INSTALL_NAMESPACE "kf6/propertiesdialog")

target_link_libraries(gdrivepropertiesplugin
    KF6::I18n
    KF6::KIOWidgets
)
