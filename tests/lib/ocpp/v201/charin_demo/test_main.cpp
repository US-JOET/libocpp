#include <boost/function/function_base.hpp>
#include <boost/optional.hpp>
#include <boost/optional/optional_io.hpp>
#include <boost/program_options/errors.hpp>
#include <boost/program_options/option.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iostream>
#include <sstream>
#include <vector>

#include "lib/ocpp/v201/smart_charging_test_utils.hpp"
#include "ocpp/common/types.hpp"
#include "ocpp/v201/ocpp_types.hpp"
#include "ocpp/v201/utils.hpp"

static const std::string CHARIN_DEFAULT_OUTPUT_PATH = "/tmp/EVerest/libocpp/";

void write_file(std::string cs_json, std::string cs_filename, boost::optional<std::string> output_directory) {
    std::string filepath = output_directory.value() + cs_filename;

    std::ofstream outfile(filepath);

    if (outfile.is_open()) {
        outfile << cs_json;
        outfile.close();
    } else {
        std::cerr << "Unable to open file";
    }
}

std::string write_files(std::string cs_filename, ocpp::v201::CompositeSchedule cs,
                        boost::optional<std::string> output_directory) {
    // Serialize the CompositeSchedule into a json string.
    std::string cs_json = ocpp::v201::utils::to_string(cs);

    std::string default_filename = "currentCompositeSchedule.json";

    write_file(cs_json, cs_filename, output_directory);
    write_file(cs_json, default_filename, output_directory);

    return cs_json;
}

std::string generate_unique_hash_filename(std::vector<ocpp::v201::ChargingProfile> profiles, ocpp::DateTime start_time,
                                          ocpp::DateTime end_time) {
    // Create a single json string from all the profiles being processed.
    std::string profiles_json = ocpp::v201::SmartChargingTestUtils::to_string(profiles);

    std::string to_be_hashed = profiles_json + start_time.to_rfc3339() + end_time.to_rfc3339();

    return ocpp::v201::SmartChargingTestUtils::filename_with_hash("CompositeSchedule", profiles_json);
}

int main(int argc, char** argv) {

    std::string input_directory;
    boost::optional<std::string> output_directory;
    ocpp::DateTime start_time;
    ocpp::DateTime end_time;
    boost::program_options::options_description desc("Allowed options");
    desc.add_options()("help",
                       "produce help message")("input-dir",
                                               boost::program_options::value<std::string>()->required()->notifier(
                                                   [&input_directory](std::string path) { input_directory = path; }),
                                               "path to the directory with json files")(
        "output-dir", boost::program_options::value<std::string>()->notifier([&output_directory](std::string path) {
            output_directory = path;
        }),
        "optional path to spit out stuff")("start-time",
                                           boost::program_options::value<std::string>()->required()->notifier(
                                               [&start_time](std::string time) { start_time = ocpp::DateTime(time); }),
                                           "the start time for calculating the composite schedule window")(
        "end-time", boost::program_options::value<std::string>()->required()->notifier([&end_time](std::string time) {
            end_time = ocpp::DateTime(time);
        }),
        "the end time for calculating the composite schedule window");

    boost::program_options::basic_command_line_parser<char> parser(argc, argv);
    boost::program_options::parsed_options parsed = parser.options(desc).allow_unregistered().run();
    boost::program_options::variables_map vm;
    boost::program_options::store(parsed, vm);

    if (vm.count("help")) {
        std::cout << desc << "\n";
        return 1;
    }

    boost::program_options::notify(vm);

    if (!output_directory.has_value()) {
        // TODO: Make sure this directory exists.
        output_directory = CHARIN_DEFAULT_OUTPUT_PATH;
    }

    std::vector<ocpp::v201::ChargingProfile> profiles =
        ocpp::v201::SmartChargingTestUtils::getChargingProfilesFromDirectory(input_directory);
    std::string profiles_json = ocpp::v201::SmartChargingTestUtils::to_string(profiles);

    ocpp::v201::SmartChargingHandler handler = ocpp::v201::SmartChargingTestUtils::smart_charging_handler_factory();

    ocpp::v201::CompositeSchedule cs =
        handler.calculate_composite_schedule(profiles, start_time, end_time, 1, ocpp::v201::ChargingRateUnitEnum::W);

    std::string cs_filename =
        ocpp::v201::SmartChargingTestUtils::filename_with_hash("CompositeSchedule", profiles_json);

    std::string filename = generate_unique_hash_filename(profiles, start_time, end_time);
    std::string cs_json = write_files(filename, cs, output_directory);

    std::cout << "composite_schedule: " << cs_json << std::endl;
    std::cout << "input_directory: " << input_directory << std::endl;
    std::cout << "output_directory: " << output_directory << std::endl;

    // testing::InitGoogleTest(&argc, argv);
    // return RUN_ALL_TESTS();
    return 0;
}
