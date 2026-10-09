# Mirror MODELS/*.hdm into DESTINATION/models_hd (a stale .hdm would still
# draw) and add ATLASES into DESTINATION/atlases, never removing one. Unchanged
# files are skipped. Invoked via `cmake -P` by the Fitd build on macOS and by
# `make models-install`.

foreach(_var DESTINATION MODELS ATLASES)
    if(NOT DEFINED ${_var})
        message(FATAL_ERROR "copy_hd_models.cmake: ${_var} is required")
    endif()
endforeach()

file(GLOB _models "${MODELS}/*.hdm")
file(GLOB _installed "${DESTINATION}/models_hd/*.hdm")
foreach(_file IN LISTS _installed)
    get_filename_component(_name "${_file}" NAME)
    if(NOT EXISTS "${MODELS}/${_name}")
        file(REMOVE "${_file}")
    endif()
endforeach()
file(MAKE_DIRECTORY "${DESTINATION}/models_hd")
if(_models)
    file(COPY ${_models} DESTINATION "${DESTINATION}/models_hd")
endif()
if(IS_DIRECTORY "${ATLASES}")
    file(COPY "${ATLASES}/" DESTINATION "${DESTINATION}/atlases" PATTERN "Backups" EXCLUDE)
endif()
list(LENGTH _models _count)
message(STATUS "models_hd: ${_count} .hdm in ${DESTINATION}")
