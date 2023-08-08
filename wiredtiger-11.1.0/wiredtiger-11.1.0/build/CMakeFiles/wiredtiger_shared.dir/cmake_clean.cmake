file(REMOVE_RECURSE
  "libwiredtiger.pdb"
  "libwiredtiger.so"
  "libwiredtiger.so.11.1.0"
)

# Per-language clean rules from dependency scanning.
foreach(lang C)
  include(CMakeFiles/wiredtiger_shared.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
