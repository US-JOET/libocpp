#include <filesystem>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <database_handler_mock.hpp>
#include <evse_security_mock.hpp>
#include <ocpp/common/call_types.hpp>
#include <ocpp/v16/smart_charging.hpp>
#include <optional>

namespace ocpp {
namespace v16 {

/**
 * Chargepoint Test Fixture
 *
 * Validate Profile Test Matrix:
 *
 * Positive Boundary Conditions:
 * - PB01 Valid Profile
 * - PB02 Valid Profile No startSchedule & handler allows no startSchedule & profile.chargingProfileKind == Absolute
 * - PB03 Valid Profile No startSchedule & handler allows no startSchedule & profile.chargingProfileKind == Relative
 * - PB04 Absolute ChargePointMaxProfile Profile with connector id 0
 * - PB05 Absolute TxDefaultProfile
 * - PB06 Absolute TxProfile ignore_no_transaction == true
 * - PB07 Absolute TxProfile && connector transaction != nullptr && transaction_id matches SKIPPED: was not able to test
 *
 * Negative Boundary Conditions:
 * - NB01 Valid Profile, ConnectorID gt this->connectors.size()
 * - NB02 Valid Profile, ConnectorID lt 0
 * - NB03 profile.stackLevel lt 0
 * - NB04 profile.stackLevel gt profile_max_stack_level
 * - NB05 profile.chargingProfileKind == Absolute && !profile.chargingSchedule.startSchedule
 * - NB06 Number of installed Profiles is > max_charging_profiles_installed
 * - NB07 Invalid ChargingSchedule
 * - NB08 profile.chargingProfileKind == Recurring && !profile.recurrencyKind
 * - NB09 profile.chargingProfileKind == Recurring && !startSchedule
 * - NB10 profile.chargingProfileKind == Recurring && !startSchedule && !allow_charging_profile_without_start_schedule
 * - NB11 Absolute ChargePointMaxProfile Profile with connector id not 0
 * - NB12 Absolute TxProfile connector_id == 0
 */
class ChargepointTestFixture : public testing::Test {
public:
    void SetUp() override {
    }

    // Adding transactiosn to the connectors because Profiles other than ChargePointMaxProfile need it in order to be valid.
    void addConnector(int id) {
        auto connector = Connector{id};

        auto timer = std::unique_ptr<Everest::SteadyTimer>();

        connector.transaction = std::make_shared<Transaction>(id, "test", "test", 1, std::nullopt, ocpp::DateTime(), std::move(timer));
        connectors[id] = std::make_shared<Connector>(connector);
    }

    ChargingSchedule createChargeSchedule() {
        return ChargingSchedule{{}};
    }

    ChargingSchedule createChargeSchedule(ChargingRateUnit chargingRateUnit) {
        std::vector<ChargingSchedulePeriod> chargingSchedulePeriod;
        std::optional<int32_t> duration;
        std::optional<ocpp::DateTime> startSchedule;
        std::optional<float> minChargingRate;

        return ChargingSchedule{chargingRateUnit, chargingSchedulePeriod, duration, startSchedule, minChargingRate};
    }

    ChargingProfile createChargingProfile(ChargingSchedule chargingSchedule) {
        return createChargingProfile(chargingSchedule, ChargingProfilePurposeType::TxDefaultProfile);
    }

    ChargingProfile createChargingProfile(ChargingSchedule chargingSchedule, ChargingProfilePurposeType chargingProfilePurpose) {
        auto chargingProfileId = 1;
        auto stackLevel = 1;
        auto chargingProfileKind = ChargingProfileKindType::Absolute;
        auto recurrencyKind = RecurrencyKindType::Daily;
        return ChargingProfile{
            chargingProfileId,
            stackLevel,
            chargingProfilePurpose,
            chargingProfileKind,
            chargingSchedule,
            {}, // transactionId
            recurrencyKind,
            {}, // validFrom
            {}  // validTo
        };
    }

    /**
     * TxDefaultProfile, stack #1: time-of-day limitation to 2 kW, recurring every day from 17:00h to 20:00h.
     *
     * This profile is Example #1 taken from the OCPP 2.0.1 Spec Part 2, page 241.
     */
    ChargingProfile createChargingProfile_Example1() {
        auto chargingRateUnit = ChargingRateUnit::W;
        auto chargingSchedulePeriod = std::vector<ChargingSchedulePeriod>{ChargingSchedulePeriod{0, 2000, 1}};
        auto duration = 1080;
        auto startSchedule = ocpp::DateTime("2024-01-17T17:00:00");
        float minChargingRate = 0;
        auto chargingSchedule =
            ChargingSchedule{chargingRateUnit, chargingSchedulePeriod, duration, startSchedule, minChargingRate};

        auto chargingProfileId = 1;
        auto stackLevel = 1;
        auto chargingProfilePurpose = ChargingProfilePurposeType::TxDefaultProfile;
        auto chargingProfileKind = ChargingProfileKindType::Absolute;
        auto recurrencyKind = RecurrencyKindType::Daily;
        return ChargingProfile{
            chargingProfileId,
            stackLevel,
            chargingProfilePurpose,
            chargingProfileKind,
            chargingSchedule,
            {}, // transactionId
            recurrencyKind,
            {}, // validFrom
            {}  // validTo
        };
    }

    /**
     * TxDefaultProfile, stack #2: overruling Sundays to no limit, recurring every week starting 2020-01-05.
     *
     * This profile is Example #2 taken from the OCPP 2.0.1 Spec Part 2, page 241.
     */
    ChargingProfile createChargingProfile_Example2() {
        auto chargingRateUnit = ChargingRateUnit::W;
        auto chargingSchedulePeriod = std::vector<ChargingSchedulePeriod>{ChargingSchedulePeriod{0, 999999, 1}};
        auto duration = 0;
        auto startSchedule = ocpp::DateTime("2020-01-19T00:00:00");
        float minChargingRate = 0;
        auto chargingSchedule =
            ChargingSchedule{chargingRateUnit, chargingSchedulePeriod, duration, startSchedule, minChargingRate};

        auto chargingProfileId = 11;
        auto stackLevel = 2;
        auto chargingProfilePurpose = ChargingProfilePurposeType::TxDefaultProfile;
        auto chargingProfileKind = ChargingProfileKindType::Recurring;
        auto recurrencyKind = RecurrencyKindType::Weekly;
        return ChargingProfile{chargingProfileId,
                               stackLevel,
                               chargingProfilePurpose,
                               chargingProfileKind,
                               chargingSchedule,
                               {}, // transactionId
                               recurrencyKind,
                               {},
                               {}};
    }

    SmartChargingHandler* createSmartChargingHandler() {
        return createSmartChargingHandler(0);
    }

    SmartChargingHandler* createSmartChargingHandler(const int number_of_connectors) {
        for (int i = 0; i <= number_of_connectors; i++) {
            addConnector(i);
        }

        const std::string chargepoint_id = "1";
        const fs::path database_path = "na";
        const fs::path init_script_path = "na";

        std::shared_ptr<DatabaseHandlerMock> database_handler =
            std::make_shared<DatabaseHandlerMock>(chargepoint_id, database_path, init_script_path);

        auto handler = new SmartChargingHandler(connectors, database_handler, true);

        return handler;
    }


    SmartChargingHandler* createSmartChargingHandlerWithChargePointMaxProfile() {
        auto profile = createChargingProfile(createChargeSchedule(ChargingRateUnit::A));
        const std::vector<ChargingRateUnit>& charging_schedule_allowed_charging_rate_units{ChargingRateUnit::A};
        auto handler = createSmartChargingHandler(10);

        profile.chargingProfilePurpose = ChargingProfilePurposeType::ChargePointMaxProfile;
        profile.chargingProfileKind = ChargingProfileKindType::Absolute;
        handler->add_charge_point_max_profile(profile);

        return handler;
    }

    // Default values used within the tests
    std::map<int32_t, std::shared_ptr<Connector>> connectors;
    std::shared_ptr<DatabaseHandler> database_handler;

    const int connector_id = 1;
    bool ignore_no_transaction = true;
    const int profile_max_stack_level = 10;
    const int max_charging_profiles_installed = 20;
    const int charging_schedule_max_periods = 10;
};

/**
 * SmartChargingHandler::add_charge_point_max_profile tests
 */
TEST_F(ChargepointTestFixture, AddChargePointMaxProfile) {
    auto profile = createChargingProfile(createChargeSchedule(ChargingRateUnit::A));
    const std::vector<ChargingRateUnit>& charging_schedule_allowed_charging_rate_units{ChargingRateUnit::A};
    auto handler = createSmartChargingHandler(10);

    profile.chargingProfilePurpose = ChargingProfilePurposeType::ChargePointMaxProfile;
    profile.chargingProfileKind = ChargingProfileKindType::Absolute;
    handler->add_charge_point_max_profile(profile);

    auto valid_profiles = handler->get_valid_profiles({}, {}, 0);

    ASSERT_EQ(1, valid_profiles.size());
    ASSERT_FALSE(false);

    // auto retrieved = valid_profiles[0];

    // ASSERT_EQ(1, valid_profiles.size());
    // ASSERT_EQ(ChargingProfilePurposeType::ChargePointMaxProfile, retrieved.chargingProfilePurpose);
    // ASSERT_EQ(ChargingProfileKindType::Absolute, retrieved.chargingProfileKind);
}


} // namespace v16
} // namespace ocpp