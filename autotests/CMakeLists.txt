include(ECMAddTests)

ecm_add_test(
    listtest.cpp
    LINK_LIBRARIES Qt5::Test KF5::KIOCore
    TEST_NAME listtest
    NAME_PREFIX kio_gdrive-)
