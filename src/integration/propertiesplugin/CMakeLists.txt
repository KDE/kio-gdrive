set(gdrivepropertiesplugin_SRCS
    gdrivepropertiesplugin.cpp
    ../../gdrivedebug.cpp
)

ki18n_wrap_ui(gdrivepropertiesplugin_UI_SRCS gdrivepropertiesplugin.ui)

add_library(gdrivepropertiesplugin MODULE ${gdrivepropertiesplugin_UI_SRCS} ${gdrivepropertiesplugin_SRCS})

target_link_libraries(gdrivepropertiesplugin
    KF5::I18n
    KF5::KIOWidgets
)

install(TARGETS gdrivepropertiesplugin DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES gdrivepropertiesplugin.desktop DESTINATION ${SERVICES_INSTALL_DIR})
