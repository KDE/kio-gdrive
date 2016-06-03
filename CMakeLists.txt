project(kio-gdrive)

cmake_minimum_required(VERSION 2.8.12)

include(FeatureSummary)

set(QT_MIN_VERSION 5.2.0)
set(KF5_MIN_VERSION 5.0.0)

find_package(Qt5 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
    Network
    Widgets)

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS
    I18n
    KIO
    Wallet)

find_package(KF5GAPI 5.0.0 REQUIRED)

add_definitions(-DQT_NO_URL_CAST_FROM_STRING)
add_definitions(-DTRANSLATION_DOMAIN=\"kio_gdrive\")

include_directories(
   ${CMAKE_SOURCE_DIR}
   ${CMAKE_BINARY_DIR}
#    ${KDE4_INCLUDES}
#    ${LibKGAPI2_INCLUDE_DIR}
)

add_subdirectory(src)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
