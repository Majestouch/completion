
add_subdirectory (ast)
add_subdirectory (cpp11)
add_subdirectory (completion)

add_executable (clidoc_main main.cc)
target_link_libraries (
  clidoc_main
  clidoc_ast_build clidoc_code_gen_logic
)

function (clidoc_cpp11_handler user_defined_program doc_file_path)
  # set filename and libname.
  get_filename_component (clidoc_cpp11_lib_name ${doc_file_path} NAME)
  set (clidoc_cpp11_file_name ${clidoc_cpp11_lib_name}-cpp11.cc)
  # cpp11 code gen.
  add_custom_command (
    OUTPUT ${clidoc_cpp11_file_name}
    COMMAND clidoc_main ARGS "cpp11" ${doc_file_path} ${clidoc_cpp11_file_name}
    DEPENDS ${doc_file_path}
  )
  # create library.
  add_library (${clidoc_cpp11_lib_name} ${clidoc_cpp11_file_name})
  set_target_properties (
    ${clidoc_cpp11_lib_name}
    PROPERTIES
    LINKER_LANGUAGE CXX
    COMPILE_FLAGS "-I ${clidoc_SOURCE_DIR}/src"
  )
  target_link_libraries (
    ${clidoc_cpp11_lib_name}
    clidoc_smart_ptr_interface
  )
  target_link_libraries (
    ${user_defined_program}
    clidoc_cpp11 ${clidoc_cpp11_lib_name}
  )
  # delete names.
  unset (clidoc_cpp11_lib_name)
  unset (clidoc_cpp11_file_name)
endfunction(clidoc_cpp11_handler)
