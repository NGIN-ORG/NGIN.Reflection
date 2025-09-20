if(NOT DEFINED SRC OR NOT DEFINED DST)
  message(FATAL_ERROR "SyncCompileCommands requires SRC and DST variables")
endif()

if(NOT EXISTS "${SRC}")
  # Nothing to copy yet; exit quietly so configure continues without errors.
  return()
endif()

get_filename_component(_dst_dir "${DST}" DIRECTORY)
if(_dst_dir AND NOT EXISTS "${_dst_dir}")
  file(MAKE_DIRECTORY "${_dst_dir}")
endif()

# Read both files to avoid unnecessary rewrites that can retrigger watchers.
file(READ "${SRC}" _src_content)
if(EXISTS "${DST}")
  file(READ "${DST}" _dst_content)
  if(_src_content STREQUAL _dst_content)
    return()
  endif()
endif()

file(COPY "${SRC}" DESTINATION "${_dst_dir}")
set(_copied "${_dst_dir}/compile_commands.json")
if(NOT "${_copied}" STREQUAL "${DST}")
  file(RENAME "${_copied}" "${DST}")
endif()
