kcoreaddons_add_plugin(purpose_gdrive SOURCES purpose_gdrive.cpp gdrivejob.cpp JSON "purpose_gdrive.json" INSTALL_NAMESPACE "kf5/purpose")
target_link_libraries(purpose_gdrive Qt5::Core KF5::Purpose)
set_target_properties(purpose_gdrive PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/kf5/purpose")
install(FILES "purpose_gdrive_config.qml" DESTINATION ${KDE_INSTALL_DATADIR}/purpose)

target_link_libraries(purpose_gdrive KF5::KIOCore KF5::I18n KF5::Purpose)
