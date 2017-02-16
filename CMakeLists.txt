project(kio-gdrive)

cmake_minimum_required(VERSION 2.8.12)

include(FeatureSummary)

set(PROJECT_VERSION 1.1.70)

set(QT_MIN_VERSION 5.2.0)
set(KF5_MIN_VERSION 5.14.0)
set(KGAPI_MIN_VERSION 5.3.1)
set(KACCOUNTS_MIN_VERSION 17.03.70)
set(QTKEYCHAIN_MIN_VERSION 0.6.0)

find_package(Qt5 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
    Gui
    Network
    Widgets)

find_package(ECM 5.14.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMQtDeclareLoggingCategory)
include(ECMSetupVersion)

find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS
    I18n
    KIO)

find_package(KF5GAPI ${KGAPI_MIN_VERSION} REQUIRED)

find_package(KAccounts ${KACCOUNTS_MIN_VERSION} REQUIRED)

find_package(Qt5Keychain ${QTKEYCHAIN_MIN_VERSION})
set_package_properties(Qt5Keychain PROPERTIES
    TYPE REQUIRED
    URL "https://github.com/frankosterfeld/qtkeychain"
    PURPOSE "Required for secure storage of accounts secrets.")

find_package(Qt5Test QUIET)
set_package_properties(Qt5Test PROPERTIES
    TYPE OPTIONAL
    PURPOSE "Required for building tests.")

if(NOT Qt5Test_FOUND)
   set(BUILD_TESTING OFF CACHE BOOL "Build the testing tree.")
endif()

ecm_setup_version(${PROJECT_VERSION}
    VARIABLE_PREFIX GDRIVE
    VERSION_HEADER gdriveversion.h)

add_definitions(-DQT_NO_URL_CAST_FROM_STRING)
add_definitions(-DQT_STRICT_ITERATORS)
add_definitions(-DTRANSLATION_DOMAIN=\"kio5_gdrive\")

add_subdirectory(autotests)
add_subdirectory(desktop)
add_subdirectory(service)
add_subdirectory(doc)
add_subdirectory(src)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
