# Builds SC4Politics.dll (32-bit Windows, MSVC). SimCity 4 is a 32-bit process: use -A Win32.
cmake_minimum_required(VERSION 3.15)
if(NOT GZCOM_DIR)
  message(FATAL_ERROR "Set -DGZCOM_DIR=<path to a checkout of nsgomez/gzcom-dll>")
endif()
set(GZ "${GZCOM_DIR}/gzcom-dll")
add_library(SC4Politics SHARED
  src/PoliticsDirector.cpp
  ${GZ}/src/SC4String.cpp
  ${GZ}/src/STLPORTCharString.cpp
  ${GZ}/src/StringResourceManager.cpp
  ${GZ}/src/cRZBaseString.cpp
  ${GZ}/src/cRZBaseVariant.cpp
  ${GZ}/src/cRZCOMDllDirector.cpp
  ${GZ}/src/cRZMessage2.cpp
  ${GZ}/src/cRZMessage2Standard.cpp
  ${GZ}/src/cS3DVector3.cpp
  ${GZ}/src/cSCBaseProperty.cpp)
target_include_directories(SC4Politics PRIVATE ${GZ}/include ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_compile_features(SC4Politics PRIVATE cxx_std_17)
target_compile_definitions(SC4Politics PRIVATE _CRT_SECURE_NO_WARNINGS WIN32_LEAN_AND_MEAN NOMINMAX)
set_target_properties(SC4Politics PROPERTIES
  MSVC_RUNTIME_LIBRARY "MultiThreaded"   # static CRT: players need no VC++ redistributable
  PREFIX "")
target_link_libraries(SC4Politics PRIVATE ws2_32)
