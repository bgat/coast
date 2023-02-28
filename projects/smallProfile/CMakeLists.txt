add_library(SmallProfile SHARED smallProfile.cpp)
target_link_libraries(SmallProfile
  "$<$<PLATFORM_ID:Darwin>:-undefined dynamic_lookup>")
