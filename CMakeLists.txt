cmake_minimum_required(VERSION 3.16)

# KDE Application Version, managed by release script
set (RELEASE_SERVICE_VERSION_MAJOR "26")
set (RELEASE_SERVICE_VERSION_MINOR "03")
set (RELEASE_SERVICE_VERSION_MICRO "70")
set (RELEASE_SERVICE_VERSION "${RELEASE_SERVICE_VERSION_MAJOR}.${RELEASE_SERVICE_VERSION_MINOR}.${RELEASE_SERVICE_VERSION_MICRO}")
project(kio-gdrive VERSION ${RELEASE_SERVICE_VERSION})

set(QT_MIN_VERSION 6.5.0)
set(KF_MIN_VERSION 6.3.0)
# TODO: bump to 6.3.0 after release/24.12 gets branched
set(KGAPI_MIN_VERSION 6.2.40)
set(KACCOUNTS_MIN_VERSION 24.08.0)
set(QTKEYCHAIN_MIN_VERSION 0.12.0)

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules" ${ECM_MODULE_PATH})

include(FeatureSummary)
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMQtDeclareLoggingCategory)
include(ECMSetupVersion)
include(KDEClangFormat)
include(KDEGitCommitHooks)

find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
    Gui
    Network
    Widgets)


find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS
    I18n
    DocTools
    KIO)

find_package(KPim6GAPI ${KGAPI_MIN_VERSION} REQUIRED)

find_package(KF6Purpose ${KF_MIN_VERSION}) # Optional since it depends on KAccounts

set_package_properties(KF6Purpose PROPERTIES TYPE RECOMMENDED PURPOSE "Enables the Share menu plugin.")

find_package(KAccounts6 ${KACCOUNTS_MIN_VERSION})
set_package_properties(KAccounts6 PROPERTIES
    TYPE RECOMMENDED
    URL "https://commits.kde.org/kaccounts-integration"
    PURPOSE "Enables integration with system-wide accounts.")

if (NOT KAccounts6_FOUND)
    find_package(Qt6Keychain ${QTKEYCHAIN_MIN_VERSION} REQUIRED)
    set_package_properties(Qt6Keychain PROPERTIES
        URL "https://github.com/frankosterfeld/qtkeychain"
        PURPOSE "Required for secure storage of accounts secrets.")
    message(STATUS "Building the QtKeychain backend.")
endif()

find_package(Qt6Test QUIET)
set_package_properties(Qt6Test PROPERTIES
    TYPE OPTIONAL
    PURPOSE "Required for building tests.")

if(NOT Qt6Test_FOUND)
   set(BUILD_TESTING OFF CACHE BOOL "Build the testing tree.")
endif()

ecm_setup_version(${RELEASE_SERVICE_VERSION}
    VARIABLE_PREFIX GDRIVE
    VERSION_HEADER gdriveversion.h)

add_definitions(-DQT_NO_URL_CAST_FROM_STRING)
add_definitions(-DQT_STRICT_ITERATORS)
add_definitions(-DTRANSLATION_DOMAIN=\"kio6_gdrive\")

if(BUILD_TESTING)
    add_subdirectory(autotests)
endif()
add_subdirectory(desktop)
add_subdirectory(doc)
add_subdirectory(src)

if (KF6Purpose_FOUND)
    add_subdirectory(purpose)
endif()

if (KAccounts6_FOUND)
    message(STATUS "Building the KAccounts backend.")
    add_subdirectory(kaccounts)
endif()

ki18n_install(po)
kdoctools_install(po)

file(GLOB_RECURSE ALL_CLANG_FORMAT_SOURCE_FILES *.cpp *.h)
kde_clang_format(${ALL_CLANG_FORMAT_SOURCE_FILES})

kde_configure_git_pre_commit_hook(CHECKS CLANG_FORMAT)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
