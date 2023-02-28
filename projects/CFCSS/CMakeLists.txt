add_library(CFCSS SHARED CFCSS.cpp)
target_link_libraries(CFCSS
  "$<$<PLATFORM_ID:Darwin>:-undefined dynamic_lookup>")
