# ── eFuse host tests ─────────────────────────────────────────────────────────
# Self-contained host unit tests, independent of the QEMU integration test in
# the parent test/ directory. Configure directly, e.g.:
#     cmake -S test/efuse -B build-efuse && ctest --test-dir build-efuse
cmake_minimum_required(VERSION 3.22)
project(esp_serial_flasher_efuse_tests C)

enable_testing()

set(REPO_ROOT ${CMAKE_CURRENT_LIST_DIR}/../..)

# Build the real flasher library (default USER_DEFINED port) and drop the single
# file that owns the register-I/O seam: esp_loader.c defines
# esp_loader_read_register / _write_register / esp_loader_get_security_info,
# which the fake replaces. Reusing the shipped target means new library sources
# flow in automatically — the tests can't drift from what the library builds.
# The unreferenced transport (protocol_*, slip, md5, stubs) is never pulled into
# a test binary thanks to static linking + --gc-sections; it only costs build
# time. The fake supplies the three seam functions; the pure-logic tests below
# reference none of them, so they need no fake.
add_subdirectory(${REPO_ROOT} flasher_build)
get_target_property(flasher_srcs flasher SOURCES)
list(FILTER flasher_srcs EXCLUDE REGEX "esp_loader\\.c$")
set_target_properties(flasher PROPERTIES SOURCES "${flasher_srcs}")
target_compile_options(flasher PRIVATE -ffunction-sections -fdata-sections)

# Applied to the test targets below (defined after this point), not to flasher
# above — so -Wall -Wextra never hits the transport sources it doesn't own.
# --gc-sections drops everything a test doesn't reference (all of the transport,
# and the register-touching engine for the pure tests).
add_compile_options(-Wall -Wextra)
add_link_options(-Wl,--gc-sections)

# Every test needs its own dir (for test_util.h) and the library's private
# headers; the library's public include dirs come in via linking flasher.
include_directories(${CMAKE_CURRENT_LIST_DIR} ${REPO_ROOT}/private_include)

# Each test_<name>.c is its own program (its own main() via TEST_MAIN), so it
# gets its own executable and ctest case. Kept separate — not one combined
# binary — so ctest reports per suite, and a test that needs the fake can link it
# while the pure ones don't. Extra sources (e.g. fake_efuse.c) go after the name.
function(add_efuse_test name)
    add_executable(test_${name} test_${name}.c ${ARGN})
    target_link_libraries(test_${name} PRIVATE flasher)
    add_test(NAME efuse_${name} COMMAND test_${name})
endfunction()

# Pure logic, no fake (the register-touching engine is never referenced).
add_efuse_test(layout)   # chip layout + KEY_PURPOSE table invariants
add_efuse_test(coding)   # RS(44,32) + ESP32 3/4 known-answer vectors

# Behavioral, on the in-memory fake (supplies the register-I/O seam).
add_efuse_test(keys fake_efuse.c)   # key routing/transforms + all-chip smoke/control flow
add_efuse_test(field fake_efuse.c)  # write_field/read_field bit packing + round trip
add_efuse_test(read fake_efuse.c)   # read_block words + RD_DIS masking
add_efuse_test(legacy_keys fake_efuse.c) # write_key_legacy: ESP32/C2 targets, halves, coding

# Commit/burn, device response on (issues real PGM_CMD/READ_CMD and
# drives the poll-and-verify/retry loop).
add_efuse_test(commit fake_efuse.c) # modern burn controllers, BLK0 split, retry
