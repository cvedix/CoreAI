cmake_minimum_required(VERSION 3.19)

function(pull_version)

execute_process(
    COMMAND git describe --tags
    WORKING_DIRECTORY "${CMAKE_PROJECT_DIR}"
    OUTPUT_VARIABLE GIT_DESCRIBE
    ERROR_QUIET
    RESULTS_VARIABLE GIT_DESCRIBE_ERR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Check if git command succeeded and output is not empty
if(NOT GIT_DESCRIBE_ERR EQUAL 0 OR "${GIT_DESCRIBE}" STREQUAL "")
    # Not a git repository or git command failed - use default version
    set(GIT_DESCRIBE "")
endif()

# Examples:
# - v1.0.3-1-g88da7dd
# - v2.0.0
# - v123.123.321-alpha.543+14-g88da7dd
if(NOT "${GIT_DESCRIBE}" STREQUAL "")
    string(REGEX MATCH "v.*\..*\..*" SEMVER_FOUND ${GIT_DESCRIBE})
else()
    set(SEMVER_FOUND "")
endif()

if("${SEMVER_FOUND}" STREQUAL "")
    # Default version when git is not available or no tags found
    set(GIT_VER_SEM "v1.1.0")
    set(GIT_VER_STR "1.1.0")
    set(GIT_VER_MAJOR 1)
    set(GIT_VER_MINOR 1)
    set(GIT_VER_BUILD 0)
    set(GIT_VER_TAIL "")
    set(GIT_VER_BASE "1.1.0")
else()
    # Parse version from git describe output
    string(REGEX MATCHALL "(\\v|\\.)[0-9+]|\\-.*" VER_NUMBERS ${GIT_DESCRIBE})

    list(LENGTH VER_NUMBERS VER_NUMBERS_LEN)
    list(GET VER_NUMBERS 0 GIT_VER_MAJOR)
    list(GET VER_NUMBERS 1 GIT_VER_MINOR)
    list(GET VER_NUMBERS 2 GIT_VER_BUILD)
    string(REGEX REPLACE "\\v([0-9+])" "\\1" GIT_VER_MAJOR ${GIT_VER_MAJOR})
    string(REGEX REPLACE "\\.([0-9+])" "\\1" GIT_VER_MINOR ${GIT_VER_MINOR})
    string(REGEX REPLACE "\\.([0-9+])" "\\1" GIT_VER_BUILD ${GIT_VER_BUILD})

    if(VER_NUMBERS_LEN GREATER_EQUAL 4)
        list(GET VER_NUMBERS 3 GIT_VER_TAIL)
        # Remove hash at the end
        string(REGEX REPLACE "\\-[A-z0-9]*$" "" GIT_VER_TAIL ${GIT_VER_TAIL})
        if(NOT "${GIT_VER_TAIL}" STREQUAL "")
            # Remove the - at the beginning
            string(REGEX REPLACE "^\\-(.*)" "\\1" GIT_VER_TAIL ${GIT_VER_TAIL})
            # Replace -BUILD_NO with +BUILD_NO
            string(REGEX REPLACE "(\\-)([0-9]*)$" "+\\2" GIT_VER_TAIL ${GIT_VER_TAIL})
        endif()
    else()
        set(GIT_VER_TAIL "")
    endif()

    string(JOIN "." GIT_VER_BASE ${GIT_VER_MAJOR} ${GIT_VER_MINOR} ${GIT_VER_BUILD})
    string(JOIN "-" GIT_VER_STR ${GIT_VER_BASE} ${GIT_VER_TAIL})
    string(JOIN "" GIT_VER_SEM "v" ${GIT_VER_STR})
endif()
message("Git describe: ${GIT_DESCRIBE}")
message("GIT_VER_SEM: ${GIT_VER_SEM}")
message("GIT_VER_STR: ${GIT_VER_STR}")
message("GIT_VER_MAJOR: ${GIT_VER_MAJOR}")
message("GIT_VER_MINOR: ${GIT_VER_MINOR}")
message("GIT_VER_BUILD: ${GIT_VER_BUILD}")
message("GIT_VER_TAIL: ${GIT_VER_TAIL}")

set(GIT_VER_SEM ${GIT_VER_SEM} PARENT_SCOPE)
set(GIT_VER_STR ${GIT_VER_STR} PARENT_SCOPE)
set(GIT_VER_BASE ${GIT_VER_BASE} PARENT_SCOPE)
set(GIT_VER_MAJOR ${GIT_VER_MAJOR} PARENT_SCOPE)
set(GIT_VER_MINOR ${GIT_VER_MINOR} PARENT_SCOPE)
set(GIT_VER_BUILD ${GIT_VER_BUILD} PARENT_SCOPE)
set(GIT_VER_TAIL ${GIT_VER_TAIL} PARENT_SCOPE)

endfunction()