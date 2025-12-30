cmake_minimum_required(VERSION 3.10)

# Root project for ch2_fl workspace
project(ch2_fl_workspace VERSION 1.0.0)

# Enable Hot Reload for MSVC compilers if supported
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

# Add subdirectories
add_subdirectory(lgplot_cmake)

# Note: esp32 project uses ESP-IDF build system, not CMake
# lgplot uses MSBuild (.vcxproj), not included here

message(STATUS "===============================================")
message(STATUS "ch2_fl Workspace")
message(STATUS "===============================================")
message(STATUS "Projects:")
message(STATUS "  - lgplot_cmake (CMake)")
message(STATUS "  - lgplot (MSBuild - open separately)")
message(STATUS "  - esp32 (ESP-IDF - open separately)")
message(STATUS "===============================================")
