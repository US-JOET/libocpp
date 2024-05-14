#include "ocpp/v201/ocpp_types.hpp"
#include "ocpp/v201/utils.hpp"
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <openssl/evp.h>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <sstream>
#include <vector>

namespace ocpp::v201 {

static const std::string BASE_JSON_PATH = "/tmp/EVerest/libocpp/v201/json/";

class SmartChargingTestUtils {
public:
    static std::vector<ChargingProfile> get_charging_profiles_from_directory(const std::string& path) {
        std::vector<ChargingProfile> profiles;
        for (const auto& entry : fs::directory_iterator(path)) {
            if (!entry.is_directory()) {
                fs::path path = entry.path();
                if (path.extension() == ".json") {
                    ChargingProfile profile = get_charging_profile_from_path(path);
                    std::cout << path << std::endl;
                    profiles.push_back(profile);
                }
            }
        }
        return profiles;
    }

    static ChargingProfile get_charging_profile_from_path(const std::string& path) {
        std::ifstream f(path.c_str());
        json data = json::parse(f);

        ChargingProfile cp;
        from_json(data, cp);
        return cp;
    }

    static ChargingProfile get_charging_profile_from_file(const std::string& filename) {
        const std::string full_path = BASE_JSON_PATH + filename;

        return get_charging_profile_from_path(full_path);
    }

    /// \brief Returns a vector of ChargingProfiles to be used as a baseline for testing core functionality
    /// of generating an EnhancedChargingSchedule.
    static std::vector<ChargingProfile> get_baseline_profile_vector() {
        return get_charging_profiles_from_directory(BASE_JSON_PATH + "baseline/");
    }

    static std::string to_string(std::vector<ChargingProfile>& profiles) {
        std::string s;
        for (auto& profile : profiles) {
            if (!s.empty())
                s += ", ";
            s += utils::to_string(profile);
        }

        return "[" + s + "]";
    }

    static std::string md5hash(std::string s) {
        unsigned char hash[MD5_DIGEST_LENGTH];

        EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
        EVP_DigestInit_ex(mdctx, EVP_md5(), NULL);
        EVP_DigestUpdate(mdctx, s.c_str(), s.size());
        EVP_DigestFinal_ex(mdctx, hash, NULL);
        EVP_MD_CTX_free(mdctx);

        std::ostringstream sout;
        for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
            sout << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
        }

        return sout.str();
    }
};

} // namespace ocpp::v201