cmake_minimum_required(VERSION 3.16)

# KDE Application Version, managed by release script
set (RELEASE_SERVICE_VERSION_MAJOR "24")
set (RELEASE_SERVICE_VERSION_MINOR "07")
set (RELEASE_SERVICE_VERSION_MICRO "70")
set (RELEASE_SERVICE_VERSION "${RELEASE_SERVICE_VERSION_MAJOR}.${RELEASE_SERVICE_VERSION_MINOR}.${RELEASE_SERVICE_VERSION_MICRO}")
project(kio-gdrive VERSION ${RELEASE_SERVICE_VERSION})

set(QT_MIN_VERSION 5.15.2)
set(KF_MIN_VERSION 5.96.0)
set(KGAPI_MIN_VERSION 5.11.41)
set(KACCOUNTS_MIN_VERSION 20.03.80)
set(QTKEYCHAIN_MIN_VERSION 0.6.0)

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules" ${ECM_MODULE_PATH})

include(FeatureSummary)
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMQtDeclareLoggingCategory)
include(ECMSetupVersion)
include(ECMDeprecationSettings)
include(KDEClangFormat)
include(KDEGitCommitHooks)

find_package(Qt${QT_MAJOR_VERSION} ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
    Gui
    Network
    Widgets)


find_package(KF${QT_MAJOR_VERSION} ${KF_MIN_VERSION} REQUIRED COMPONENTS
    I18n
    DocTools
    KIO)

if(QT_MAJOR_VERSION STREQUAL "5")
    find_package(KPimGAPI ${KGAPI_MIN_VERSION} REQUIRED)
else()
    find_package(KPim6GAPI ${KGAPI_MIN_VERSION} REQUIRED)
endif()

find_package(KF${QT_MAJOR_VERSION}Purpose ${KF_MIN_VERSION}) # Optional since it depends on KAccounts

set_package_properties(KF${QT_MAJOR_VERSION}Purpose PROPERTIES TYPE RECOMMENDED PURPOSE "Enables the Share menu plugin.")

if(QT_MAJOR_VERSION STREQUAL "5")
  set(KACCOUNTS_SUFFIX "")
else()
  set(KACCOUNTS_SUFFIX "6")
endif()

find_package(KAccounts${KACCOUNTS_SUFFIX} ${KACCOUNTS_MIN_VERSION})
set_package_properties(KAccounts${KACCOUNTS_SUFFIX} PROPERTIES
    TYPE RECOMMENDED
    URL "https://commits.kde.org/kaccounts-integration"
    PURPOSE "Enables integration with system-wide accounts.")

if (NOT KAccounts${KACCOUNTS_SUFFIX}_FOUND)
    find_package(Qt${QT_MAJOR_VERSION}Keychain ${QTKEYCHAIN_MIN_VERSION} REQUIRED)
    set_package_properties(Qt${QT_MAJOR_VERSION}Keychain PROPERTIES
        URL "https://github.com/frankosterfeld/qtkeychain"
        PURPOSE "Required for secure storage of accounts secrets.")
    message(STATUS "Building the QtKeychain backend.")
endif()

find_package(Qt${QT_MAJOR_VERSION}Test QUIET)
set_package_properties(Qt${QT_MAJOR_VERSION}Test PROPERTIES
    TYPE OPTIONAL
    PURPOSE "Required for building tests.")

if(NOT Qt${QT_MAJOR_VERSION}Test_FOUND)
   set(BUILD_TESTING OFF CACHE BOOL "Build the testing tree.")
endif()

ecm_setup_version(${RELEASE_SERVICE_VERSION}
    VARIABLE_PREFIX GDRIVE
    VERSION_HEADER gdriveversion.h)

add_definitions(-DQT_NO_URL_CAST_FROM_STRING)
add_definitions(-DQT_STRICT_ITERATORS)
add_definitions(-DTRANSLATION_DOMAIN=\"kio5_gdrive\")

ecm_set_disabled_deprecation_versions(QT 5.15.2
     KF 5.83.0
    )

if(BUILD_TESTING)
    add_subdirectory(autotests)
endif()
add_subdirectory(desktop)
add_subdirectory(doc)
add_subdirectory(src)

if (KF${QT_MAJOR_VERSION}Purpose_FOUND)
    add_subdirectory(purpose)
endif()

if (KAccounts${KACCOUNTS_SUFFIX}_FOUND)
    message(STATUS "Building the KAccounts backend.")
    add_subdirectory(kaccounts)
endif()

ki18n_install(po)
kdoctools_install(po)

file(GLOB_RECURSE ALL_CLANG_FORMAT_SOURCE_FILES *.cpp *.h)
kde_clang_format(${ALL_CLANG_FORMAT_SOURCE_FILES})

kde_configure_git_pre_commit_hook(CHECKS CLANG_FORMAT)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
