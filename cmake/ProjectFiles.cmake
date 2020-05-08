include_guard( DIRECTORY )

list(APPEND flex_meta_demo_SOURCES
  #${flex_meta_demo_src_DIR}/main.cc
  ${flextool_outdir}/main.cc.generated
)
