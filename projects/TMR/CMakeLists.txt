add_library(TMR SHARED TMR.cpp)
target_link_libraries(TMR
  "$<$<PLATFORM_ID:Darwin>:-undefined dynamic_lookup>")
