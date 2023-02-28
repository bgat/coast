add_library(DWC SHARED DWC.cpp)
target_link_libraries(DWC
  "$<$<PLATFORM_ID:Darwin>:-undefined dynamic_lookup>")
