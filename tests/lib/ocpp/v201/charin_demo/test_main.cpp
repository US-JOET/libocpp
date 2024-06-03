#include <boost/optional.hpp>
#include <boost/optional/optional_io.hpp>
#include <boost/program_options/option.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <gtest/gtest.h>
#include <string>
 
int main(int argc, char ** argv) {

    std::string input_directory;
    boost::optional<std::string> output_directory;
    boost::program_options::options_description desc("Allowed options");
    desc.add_options() 
        ("help", "produce help message")
        ("input-dir", boost::program_options::value<std::string>()->required(), "path to the directory with json files")
        ("output-dir", boost::program_options::value<std::string>(), "optional path to spit out stuff")
    ;

    boost::program_options::basic_command_line_parser<char> parser(argc, argv);
    boost::program_options::parsed_options parsed = parser.options(desc).allow_unregistered().run();
    boost::program_options::variables_map vm;
    boost::program_options::store(parsed, vm);

    if (vm.count("help"))
        {
            std::cout << desc << "\n";
            return 1;
        }

    boost::program_options::notify(vm);
    for (auto option : parsed.options) {
        std::cout << "HERE IS AN OPTION: " << option.original_tokens.back() << " UNREGISTERED? " << option.unregistered << std::endl;
    }

    if (output_directory) {
        std::cout << "output_directory is specified as: " << output_directory << std::endl;
    } else {
        std::cout << "No output_directory specified. Using default output" << std::endl;
    }

    std::cout << "DESC: " << desc << std::endl;
    
    // testing::InitGoogleTest(&argc, argv);
    // return RUN_ALL_TESTS();
    return 0;
}