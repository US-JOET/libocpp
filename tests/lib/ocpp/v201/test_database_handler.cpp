
#include "ocpp/v201/database_handler.hpp"
#include "ocpp/v201/ocpp_types.hpp"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>

namespace ocpp::v201 {

class DatabaseHandlerV201 : public testing::Test {

public:
    DatabaseHandlerV201() {
        std::unique_ptr<common::DatabaseConnection> dbc = std::make_unique<common::DatabaseConnection>("file::memory:?cache=shared");
        dbc->open_connection(); // Open connection so memory stays shared

        this->db_handler = std::make_unique<DatabaseHandler>(std::move(dbc),
                                                             std::filesystem::path(MIGRATION_FILES_LOCATION_V201));
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
    db_handler->insert_or_update_charging_profile(1, ChargingProfile{1,1});
    std::string sql = "SELECT COUNT(*) FROM CHARGING_PROFILES";
    auto select_stmt = db_connection->new_statement(sql);
    
    if (select_stmt->step() != SQLITE_ROW) {
        // TODO ...
        return;
    }

    auto count = select_stmt->column_int(0);
    ASSERT_EQ(count, 1);
}

TEST_F(DatabaseHandlerV201, KO1_FR27_DatabaseWithProfileData_UpdateProfile) {
    db_handler->insert_or_update_charging_profile(1, ChargingProfile{2,1});
    db_handler->insert_or_update_charging_profile(1, ChargingProfile{2,2});

    std::string sql = "SELECT COUNT(*) FROM CHARGING_PROFILES";
    auto select_stmt = db_connection->new_statement(sql);
    
    if (select_stmt->step() != SQLITE_ROW) {
        // TODO ...
        return;
    }

    auto count = select_stmt->column_int(0);
    ASSERT_EQ(count, 1);
}

TEST_F(DatabaseHandlerV201, KO1_FR27_DatabaseWithProfileData_InsertNewProfile) {
    db_handler->insert_or_update_charging_profile(1, ChargingProfile{1,1});
    db_handler->insert_or_update_charging_profile(1, ChargingProfile{2,1});

    std::string sql = "SELECT COUNT(*) FROM CHARGING_PROFILES";
    auto select_stmt = db_connection->new_statement(sql);
    
    if (select_stmt->step() != SQLITE_ROW) {
        // TODO ...
        return;
    }

    auto count = select_stmt->column_int(0);
    ASSERT_EQ(count, 2);
}

} // namespace ocpp::v201