add_library(DataflowProtection SHARED
  dataflowProtection.cpp
  cloning.cpp
  synchronization.cpp
  utils.cpp
  verification.cpp
  interface.cpp
  inspection.cpp
  dataflowProtection.h
)
target_link_libraries(DataflowProtection
  "$<$<PLATFORM_ID:Darwin>:-undefined dynamic_lookup>")
