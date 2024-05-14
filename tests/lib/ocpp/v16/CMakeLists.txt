target_sources(libocpp_unit_tests PRIVATE
        profile_tests_common.cpp
        profile_testsA.cpp
        profile_testsB.cpp
        profile_testsC.cpp
        test_database_migration_files.cpp
        test_smart_charging_handler.cpp
        test_composite_schedule.cpp
        test_database_migration_files.cpp
        test_smart_charging_handler.cpp
)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/json DESTINATION /tmp/EVerest/libocpp/v16)
