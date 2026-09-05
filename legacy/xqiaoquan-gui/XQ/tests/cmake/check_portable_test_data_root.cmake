if(NOT DEFINED XQ_SOURCE_DIR)
    message(FATAL_ERROR "XQ_SOURCE_DIR must be defined")
endif()

file(READ "${XQ_SOURCE_DIR}/CMakeLists.txt" _xq_cmakelists)

set(_xq_forbidden_root "C:/Users")
string(APPEND _xq_forbidden_root "/OCEAN/Desktop")
string(APPEND _xq_forbidden_root "/XIAOQUAN/0007_H_AO_H")

string(FIND "${_xq_cmakelists}" "${_xq_forbidden_root}" _xq_forbidden_index)
if(NOT _xq_forbidden_index EQUAL -1)
    message(FATAL_ERROR
        "CMakeLists.txt hard-codes the local 0007_H_AO_H path; use XQ_TEST_DATA_ROOT")
endif()

string(FIND "${_xq_cmakelists}" "XQ_TEST_DATA_ROOT" _xq_data_root_index)
if(_xq_data_root_index EQUAL -1)
    message(FATAL_ERROR
        "CMakeLists.txt must expose XQ_TEST_DATA_ROOT for real sample data tests")
endif()
