include_guard( DIRECTORY )

list(APPEND flex_meta_plugin_SOURCES
  ${flex_meta_plugin_src_DIR}/plugin_main.cc
  ${flex_meta_plugin_src_DIR}/EventHandler.hpp
  ${flex_meta_plugin_src_DIR}/EventHandler.cc
  ${flex_meta_plugin_src_DIR}/Tooling.hpp
  ${flex_meta_plugin_src_DIR}/Tooling.cc
)
