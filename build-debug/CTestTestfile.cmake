# CMake generated Testfile for 
# Source directory: /home/arjith/dev/northpole/payment_handler
# Build directory: /home/arjith/dev/northpole/payment_handler/build-debug
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[test_banks]=] "/home/arjith/dev/northpole/payment_handler/build-debug/test_banks")
set_tests_properties([=[test_banks]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/arjith/dev/northpole/payment_handler/CMakeLists.txt;45;add_test;/home/arjith/dev/northpole/payment_handler/CMakeLists.txt;0;")
add_test([=[test_db]=] "/home/arjith/dev/northpole/payment_handler/build-debug/test_db")
set_tests_properties([=[test_db]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/arjith/dev/northpole/payment_handler/CMakeLists.txt;45;add_test;/home/arjith/dev/northpole/payment_handler/CMakeLists.txt;0;")
add_test([=[test_processor]=] "/home/arjith/dev/northpole/payment_handler/build-debug/test_processor")
set_tests_properties([=[test_processor]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/arjith/dev/northpole/payment_handler/CMakeLists.txt;45;add_test;/home/arjith/dev/northpole/payment_handler/CMakeLists.txt;0;")
add_test([=[test_queue]=] "/home/arjith/dev/northpole/payment_handler/build-debug/test_queue")
set_tests_properties([=[test_queue]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/arjith/dev/northpole/payment_handler/CMakeLists.txt;45;add_test;/home/arjith/dev/northpole/payment_handler/CMakeLists.txt;0;")
add_test([=[test_runtime]=] "/home/arjith/dev/northpole/payment_handler/build-debug/test_runtime")
set_tests_properties([=[test_runtime]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/arjith/dev/northpole/payment_handler/CMakeLists.txt;45;add_test;/home/arjith/dev/northpole/payment_handler/CMakeLists.txt;0;")
add_test([=[test_stream]=] "/home/arjith/dev/northpole/payment_handler/build-debug/test_stream")
set_tests_properties([=[test_stream]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/arjith/dev/northpole/payment_handler/CMakeLists.txt;45;add_test;/home/arjith/dev/northpole/payment_handler/CMakeLists.txt;0;")
add_test([=[test_types]=] "/home/arjith/dev/northpole/payment_handler/build-debug/test_types")
set_tests_properties([=[test_types]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/arjith/dev/northpole/payment_handler/CMakeLists.txt;45;add_test;/home/arjith/dev/northpole/payment_handler/CMakeLists.txt;0;")
subdirs("_deps/nlohmann_json-build")
subdirs("_deps/doctest-build")
