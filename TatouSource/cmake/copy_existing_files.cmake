# Copy the files listed in FILES into DESTINATION, skipping any that do not
# exist. Invoked via `cmake -P` from the Fitd macOS POST_BUILD step.
#
# FILES is a single "|"-separated string so the shell cannot split the list on
# semicolons.

if(NOT DEFINED DESTINATION)
    message(FATAL_ERROR "copy_existing_files.cmake: DESTINATION is required")
endif()

file(MAKE_DIRECTORY "${DESTINATION}")

if(NOT DEFINED FILES OR FILES STREQUAL "")
    return()
endif()

string(REPLACE "|" ";" _files "${FILES}")
foreach(_file IN LISTS _files)
    if(EXISTS "${_file}")
        file(COPY "${_file}" DESTINATION "${DESTINATION}")
    endif()
endforeach()
