project(kio-gdrive)

cmake_minimum_required(VERSION 2.6)

find_package(Qt4 REQUIRED)
find_package(KDE4 REQUIRED)
find_package(LibKGAPI2 REQUIRED)
include(KDE4Defaults)

add_definitions (${QT_DEFINITIONS} ${KDE4_DEFINITIONS})

include_directories(
   ${CMAKE_SOURCE_DIR}
   ${CMAKE_BINARY_DIR}
   ${KDE4_INCLUDES}
   ${LibKGAPI2_INCLUDE_DIR}
)

add_subdirectory(src)

install(FILES gdrive.protocol DESTINATION ${KDE4_SERVICES_INSTALL_DIR})

