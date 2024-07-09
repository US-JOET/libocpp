#include <cstdint>

namespace ocpp {

// Time
const int HOURS_PER_DAY = 24;
const int SECONDS_PER_HOUR = 3600;
const int SECONDS_PER_DAY = 86400;
const int DAYS_PER_WEEK = 7;

constexpr float DEFAULT_LIMIT_AMPS = 48.0;
constexpr float DEFAULT_LIMIT_WATTS = 33120.0;
constexpr std::int32_t DEFAULT_AND_MAX_NUMBER_PHASES = 3;
constexpr float LOW_VOLTAGE = 230;

constexpr float NO_LIMIT_SPECIFIED = -1.0;

} // namespace ocpp