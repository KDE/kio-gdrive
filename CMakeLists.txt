project(kio-gdrive)

cmake_minimum_required(VERSION 2.8.12)

set(QT_MIN_VERSION 5.0.0)
set(KF5_MIN_VERSION 5.0.0)

find_package(Qt5 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS Network)

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

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_C_COMPILER MATCHES "icc" OR (CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
    # C++11!
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
    # Aggreesive warnings
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
endif()

include_directories(
   ${CMAKE_SOURCE_DIR}
   ${CMAKE_BINARY_DIR}
#    ${KDE4_INCLUDES}
#    ${LibKGAPI2_INCLUDE_DIR}
)

add_subdirectory(src)

install(FILES gdrive.protocol DESTINATION ${SERVICES_INSTALL_DIR})

