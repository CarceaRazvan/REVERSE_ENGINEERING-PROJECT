add_executable(TestYara test_yara.cpp)

# Leag YARA la target
target_link_libraries(TestYara PRIVATE YARA::YARA)
