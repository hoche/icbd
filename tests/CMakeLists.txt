cmake_minimum_required(VERSION 3.16)

#
# Test suite for icbd
#
# - Unit tests: small C executables run via CTest
# - Integration tests: black-box tests that start icbd and speak the protocol
#

enable_testing()

set(ICBD_TESTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# -----------------------
# Unit tests (C / CTest)
# -----------------------

add_executable(icbd_unit_pktbuffers
  "${ICBD_TESTS_DIR}/unit/test_pktbuffers.c"
)
target_include_directories(icbd_unit_pktbuffers PRIVATE
  "${GENERATED_DIR}"
  "${CMAKE_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/pktserv"
)
target_link_libraries(icbd_unit_pktbuffers PRIVATE pktserv)
add_test(NAME icbd.unit.pktbuffers COMMAND icbd_unit_pktbuffers)

add_executable(icbd_unit_wildmat
  "${ICBD_TESTS_DIR}/unit/test_wildmat.c"
  "${CMAKE_SOURCE_DIR}/server/wildmat.c"
)
target_include_directories(icbd_unit_wildmat PRIVATE
  "${CMAKE_SOURCE_DIR}"
)
add_test(NAME icbd.unit.wildmat COMMAND icbd_unit_wildmat)

# ------------------------------
# Integration tests (Python3)
# ------------------------------

find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
  add_test(
    NAME icbd.integration.login
    COMMAND
      "${Python3_EXECUTABLE}"
      "${ICBD_TESTS_DIR}/integration/test_login_smoke.py"
      "--icbd" "$<TARGET_FILE:icbd>"
      "--fixtures" "${CMAKE_SOURCE_DIR}/prod"
  )

  add_test(
    NAME icbd.integration.commands.clear
    COMMAND
      "${Python3_EXECUTABLE}"
      "${ICBD_TESTS_DIR}/integration/test_commands.py"
      "--icbd" "$<TARGET_FILE:icbd>"
      "--fixtures" "${CMAKE_SOURCE_DIR}/prod"
  )

  add_test(
    NAME icbd.integration.messaging.clear
    COMMAND
      "${Python3_EXECUTABLE}"
      "${ICBD_TESTS_DIR}/integration/test_messaging.py"
      "--icbd" "$<TARGET_FILE:icbd>"
      "--fixtures" "${CMAKE_SOURCE_DIR}/prod"
  )

  if(HAVE_SSL)
    add_test(
      NAME icbd.integration.commands.tls
      COMMAND
        "${Python3_EXECUTABLE}"
        "${ICBD_TESTS_DIR}/integration/test_commands.py"
        "--tls"
        "--icbd" "$<TARGET_FILE:icbd>"
        "--fixtures" "${CMAKE_SOURCE_DIR}/prod"
    )

    add_test(
      NAME icbd.integration.messaging.tls
      COMMAND
        "${Python3_EXECUTABLE}"
        "${ICBD_TESTS_DIR}/integration/test_messaging.py"
        "--tls"
        "--icbd" "$<TARGET_FILE:icbd>"
        "--fixtures" "${CMAKE_SOURCE_DIR}/prod"
    )
  endif()
else()
  message(STATUS "Python3 interpreter not found; integration tests will be skipped.")
endif()

