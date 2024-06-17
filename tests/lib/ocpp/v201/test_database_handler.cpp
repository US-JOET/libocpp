
#include "ocpp/v201/database_handler.hpp"
#include "ocpp/v201/ocpp_types.hpp"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>

namespace ocpp::v201 {

class DatabaseHandlerV201 : public testing::Test {

public:
    DatabaseHandlerV201() {
        std::unique_ptr<common::DatabaseConnection> dbc =
            std::make_unique<common::DatabaseConnection>("file::memory:?cache=shared");
        dbc->open_connection(); // Open connection so memory stays shared

        this->db_handler =
            std::make_unique<DatabaseHandler>(std::move(dbc), std::filesystem::path(MIGRATION_FILES_LOCATION_V201));
        this->db_handler->open_connection();
        this->db_connection = std::make_unique<common::DatabaseConnection>("file::memory:?cache=shared");
        db_connection->open_connection(); // Open connection so memory stays shared
    }
    std::unique_ptr<common::DatabaseConnection> db_connection;
    std::unique_ptr<DatabaseHandler> db_handler;

protected:
    void SetUp() override {
    }
};

TEST_F(DatabaseHandlerV201, KO1_FR27_DatabaseWithNoData_InsertProfile) {
    db_handler->insert_or_update_charging_profile(1, ChargingProfile{1, 1});
    std::string sql = "SELECT COUNT(*) FROM CHARGING_PROFILES";
    auto select_stmt = db_connection->new_statement(sql);

    ASSERT_TRUE(select_stmt->step() == SQLITE_ROW);
    auto count = select_stmt->column_int(0);
    ASSERT_EQ(count, 1);
}

TEST_F(DatabaseHandlerV201, KO1_FR27_DatabaseWithProfileData_UpdateProfile) {
    db_handler->insert_or_update_charging_profile(1, ChargingProfile{2, 1});
    db_handler->insert_or_update_charging_profile(1, ChargingProfile{2, 2});

    std::string sql = "SELECT COUNT(*) FROM CHARGING_PROFILES";
    auto select_stmt = db_connection->new_statement(sql);

    ASSERT_TRUE(select_stmt->step() == SQLITE_ROW);

    auto count = select_stmt->column_int(0);
    ASSERT_EQ(count, 1);
}

TEST_F(DatabaseHandlerV201, KO1_FR27_DatabaseWithProfileData_InsertNewProfile) {
    db_handler->insert_or_update_charging_profile(1, ChargingProfile{1, 1});
    db_handler->insert_or_update_charging_profile(1, ChargingProfile{2, 1});

    std::string sql = "SELECT COUNT(*) FROM CHARGING_PROFILES";
    auto select_stmt = db_connection->new_statement(sql);

    ASSERT_TRUE(select_stmt->step() == SQLITE_ROW);

    auto count = select_stmt->column_int(0);
    ASSERT_EQ(count, 2);
}

TEST_F(DatabaseHandlerV201, KO1_FR27_DatabaseWithProfileData_DeleteRemovesSpecifiedProfiles) {
    db_handler->insert_or_update_charging_profile(1, ChargingProfile{.id = 1, .stackLevel = 1});
    db_handler->insert_or_update_charging_profile(1, ChargingProfile{.id = 2, .stackLevel = 1});

    auto sql = "SELECT COUNT(*) FROM CHARGING_PROFILES";

    auto select_stmt = db_connection->new_statement(sql);

    do {
        ASSERT_TRUE(select_stmt->step() == SQLITE_ROW);
        auto count = select_stmt->column_int(0);
        ASSERT_EQ(count, 2);
    } while (select_stmt->step() != SQLITE_DONE);

    db_handler->delete_charging_profile(1);

    do {
        ASSERT_TRUE(select_stmt->step() == SQLITE_ROW);
        auto count = select_stmt->column_int(0);
        ASSERT_EQ(count, 1);
    } while (select_stmt->step() != SQLITE_DONE);
}

TEST_F(DatabaseHandlerV201, KO1_FR27_DatabaseWithProfileData_DeleteAllRemovesAllProfiles) {
    db_handler->insert_or_update_charging_profile(1, ChargingProfile{.id = 1, .stackLevel = 1});
    db_handler->insert_or_update_charging_profile(1, ChargingProfile{.id = 2, .stackLevel = 1});

    auto sql = "SELECT COUNT(*) FROM CHARGING_PROFILES";

    auto select_stmt = db_connection->new_statement(sql);

    do {
        ASSERT_TRUE(select_stmt->step() == SQLITE_ROW);
        auto count = select_stmt->column_int(0);
        ASSERT_EQ(count, 2);
    } while (select_stmt->step() != SQLITE_DONE);

    db_handler->delete_charging_profiles();

    do {
        ASSERT_TRUE(select_stmt->step() == SQLITE_ROW);
        auto count = select_stmt->column_int(0);
        ASSERT_EQ(count, 0);
    } while (select_stmt->step() != SQLITE_DONE);
}

TEST_F(DatabaseHandlerV201, KO1_FR27_DatabaseWithNoProfileData_DeleteAllDoesNotFail) {

    auto sql = "SELECT COUNT(*) FROM CHARGING_PROFILES";

    auto select_stmt = db_connection->new_statement(sql);

    do {
        ASSERT_TRUE(select_stmt->step() == SQLITE_ROW);
        auto count = select_stmt->column_int(0);
        ASSERT_EQ(count, 0);
    } while (select_stmt->step() != SQLITE_DONE);

    db_handler->delete_charging_profiles();

    do {
        ASSERT_TRUE(select_stmt->step() == SQLITE_ROW);
        auto count = select_stmt->column_int(0);
        ASSERT_EQ(count, 0);
    } while (select_stmt->step() != SQLITE_DONE);
}

TEST_F(DatabaseHandlerV201, KO1_FR27_DatabaseNoProfileData_DeleteAllDoesNotFail) {
    auto sql = "SELECT COUNT(*) FROM CHARGING_PROFILES";

    auto select_stmt = db_connection->new_statement(sql);

    do {
        ASSERT_TRUE(select_stmt->step() == SQLITE_ROW);
        auto count = select_stmt->column_int(0);
        ASSERT_EQ(count, 0);
    } while (select_stmt->step() != SQLITE_DONE);

    db_handler->delete_charging_profiles();

    do {
        ASSERT_TRUE(select_stmt->step() == SQLITE_ROW);
        auto count = select_stmt->column_int(0);
        ASSERT_EQ(count, 0);
    } while (select_stmt->step() != SQLITE_DONE);
}

TEST_F(DatabaseHandlerV201, KO1_FR27_DatabaseWithSingleProfileData_LoadsCharingProfile) {
    db_handler->insert_or_update_charging_profile(1, ChargingProfile{1, 1});

    
    auto sut = db_handler->get_all_charging_profiles_by_evse();

    ASSERT_EQ(sut.size(), 1);

    // The evse id is found
    ASSERT_FALSE(sut.find(1) == sut.end());

    auto profiles = sut[1];

    ASSERT_EQ(profiles.size(), 1);

}

TEST_F(DatabaseHandlerV201, KO1_FR27_DatabaseWithMultipleProfileSameEvse_LoadsCharingProfile) {
    db_handler->insert_or_update_charging_profile(1, ChargingProfile{.id = 1, .stackLevel = 1});
    db_handler->insert_or_update_charging_profile(1, ChargingProfile{.id = 2, .stackLevel = 2});
    db_handler->insert_or_update_charging_profile(1, ChargingProfile{.id = 3, .stackLevel = 3});

    
    auto sut = db_handler->get_all_charging_profiles_by_evse();

    ASSERT_EQ(sut.size(), 1);

    // The evse id is found
    ASSERT_FALSE(sut.find(1) == sut.end());

    auto profiles = sut[1];

    ASSERT_EQ(profiles.size(), 3);

}

TEST_F(DatabaseHandlerV201, KO1_FR27_DatabaseWithMultipleProfileDiffEvse_LoadsCharingProfile) {
    db_handler->insert_or_update_charging_profile(1, ChargingProfile{.id = 1, .stackLevel = 1});
    db_handler->insert_or_update_charging_profile(2, ChargingProfile{.id = 2, .stackLevel = 2});
    db_handler->insert_or_update_charging_profile(3, ChargingProfile{.id = 3, .stackLevel = 3});

    auto sut = db_handler->get_all_charging_profiles_by_evse();

    ASSERT_EQ(sut.size(), 3); 
  
    ASSERT_FALSE(sut.find(1) == sut.end()); 
    ASSERT_FALSE(sut.find(2) == sut.end()); 
    ASSERT_FALSE(sut.find(3) == sut.end()); 

    auto profiles1 = sut[1];
    auto profiles2 = sut[2];
    auto profiles3 = sut[3];

    ASSERT_EQ(profiles1.size(), 1);
    ASSERT_EQ(profiles2.size(), 1);
    ASSERT_EQ(profiles3.size(), 1);

}

} // namespace ocpp::v201
