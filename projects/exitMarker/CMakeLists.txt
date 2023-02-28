add_library(ExitMarker SHARED exitMarker.cpp)
target_link_libraries(ExitMarker
  "$<$<PLATFORM_ID:Darwin>:-undefined dynamic_lookup>")
