cmake_minimum_required(VERSION 3.0)

# KDE Application Version, managed by release script
set (RELEASE_SERVICE_VERSION_MAJOR "21")
set (RELEASE_SERVICE_VERSION_MINOR "12")
set (RELEASE_SERVICE_VERSION_MICRO "1")
set (RELEASE_SERVICE_VERSION "${RELEASE_SERVICE_VERSION_MAJOR}.${RELEASE_SERVICE_VERSION_MINOR}.${RELEASE_SERVICE_VERSION_MICRO}")
project(kio-gdrive VERSION ${RELEASE_SERVICE_VERSION})

set(QT_MIN_VERSION 5.2.0)
set(KF5_MIN_VERSION 5.71.0)
set(KGAPI_MIN_VERSION 5.11.41)
set(KACCOUNTS_MIN_VERSION 20.03.80)
set(QTKEYCHAIN_MIN_VERSION 0.6.0)

find_package(Qt5 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
    Gui
    Network
    Widgets)

find_package(ECM ${KF5_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules" ${ECM_MODULE_PATH})

include(FeatureSummary)
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMQtDeclareLoggingCategory)
include(ECMSetupVersion)

find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS
    I18n
    DocTools
    KIO)

find_package(KPimGAPI ${KGAPI_MIN_VERSION} REQUIRED)

find_package(KF5Purpose ${KF5_MIN_VERSION}) # Optional since it depends on KAccounts

set_package_properties(KF5Purpose PROPERTIES TYPE RECOMMENDED PURPOSE "Enables the Share menu plugin.")

find_package(KAccounts ${KACCOUNTS_MIN_VERSION})
set_package_properties(KAccounts PROPERTIES
    TYPE RECOMMENDED
    URL "https://commits.kde.org/kaccounts-integration"
    PURPOSE "Enables integration with system-wide accounts.")

if (NOT KAccounts_FOUND)
    find_package(Qt5Keychain ${QTKEYCHAIN_MIN_VERSION} REQUIRED)
    set_package_properties(Qt5Keychain PROPERTIES
        URL "https://github.com/frankosterfeld/qtkeychain"
        PURPOSE "Required for secure storage of accounts secrets.")
    message(STATUS "Building the QtKeychain backend.")
endif()

find_package(Qt5Test QUIET)
set_package_properties(Qt5Test PROPERTIES
    TYPE OPTIONAL
    PURPOSE "Required for building tests.")

if(NOT Qt5Test_FOUND)
   set(BUILD_TESTING OFF CACHE BOOL "Build the testing tree.")
endif()

ecm_setup_version(${RELEASE_SERVICE_VERSION}
    VARIABLE_PREFIX GDRIVE
    VERSION_HEADER gdriveversion.h)

add_definitions(-DQT_NO_URL_CAST_FROM_STRING)
add_definitions(-DQT_STRICT_ITERATORS)
add_definitions(-DTRANSLATION_DOMAIN=\"kio5_gdrive\")

if(BUILD_TESTING)
    add_subdirectory(autotests)
endif()
add_subdirectory(desktop)
add_subdirectory(doc)
add_subdirectory(src)

if (KF5Purpose_FOUND)
    add_subdirectory(purpose)
endif()

if (KAccounts_FOUND)
    message(STATUS "Building the KAccounts backend.")
    add_subdirectory(kaccounts)
endif()


feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
