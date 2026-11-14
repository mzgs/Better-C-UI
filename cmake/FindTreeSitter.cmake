# Upstream installs headers and libraries, without a CMake package config.
find_path(TreeSitter_INCLUDE_DIR NAMES tree_sitter/api.h)
find_library(TreeSitter_LIBRARY NAMES tree-sitter)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(TreeSitter REQUIRED_VARS TreeSitter_INCLUDE_DIR TreeSitter_LIBRARY)
mark_as_advanced(TreeSitter_INCLUDE_DIR TreeSitter_LIBRARY)
if(TreeSitter_FOUND AND NOT TARGET TreeSitter::TreeSitter)
    add_library(TreeSitter::TreeSitter UNKNOWN IMPORTED GLOBAL)
    set_target_properties(TreeSitter::TreeSitter PROPERTIES
        IMPORTED_LOCATION "${TreeSitter_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${TreeSitter_INCLUDE_DIR}")
endif()
