add_library(DebugStatements SHARED debugStatements.cpp)
target_link_libraries(DebugStatements
  "$<$<PLATFORM_ID:Darwin>:-undefined dynamic_lookup>")
