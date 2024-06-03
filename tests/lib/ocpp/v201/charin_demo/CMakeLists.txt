FIND_PACKAGE( Boost 1.74 COMPONENTS program_options REQUIRED )
INCLUDE_DIRECTORIES( ${Boost_INCLUDE_DIR} )

target_sources(charin_demo_bin PRIVATE
        test_main.cpp)

target_link_libraries(charin_demo_bin LINK_PUBLIC ${Boost_LIBRARIES} )