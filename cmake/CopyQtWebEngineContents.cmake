cmake_minimum_required(VERSION 3.15)

if (NOT DEFINED QT_SOURCE_DIR)
  message(FATAL_ERROR "QT_SOURCE_DIR must be defined")
endif()

if (NOT DEFINED QT_DEST_DIR)
  message(FATAL_ERROR "QT_DEST_DIR must be defined")
endif()

file(GLOB_RECURSE QT_RESOURCE_FILES
     RELATIVE "${QT_SOURCE_DIR}"
     "${QT_SOURCE_DIR}/*")

foreach(relative_path IN LISTS QT_RESOURCE_FILES)
  set(source "${QT_SOURCE_DIR}/${relative_path}")
  if (IS_DIRECTORY "${source}")
    continue()
  endif()

  set(destination "${QT_DEST_DIR}/${relative_path}")
  get_filename_component(destination_dir "${destination}" DIRECTORY)
  if (NOT EXISTS "${destination_dir}")
    file(MAKE_DIRECTORY "${destination_dir}")
  endif()

  execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different "${source}" "${destination}")
endforeach()


