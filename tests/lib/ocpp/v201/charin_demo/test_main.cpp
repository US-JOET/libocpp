#include <gtest/gtest.h>
 
int main(int argc, char** argv) {

    std::cout << "ARGC: " << argc << " and ARGV: " << std::endl;
    for (int arg = 0; arg < argc; ++arg) {
        std::cout << "---arg # " << arg << ": " << argv[arg] << std::endl;
    }


    
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}