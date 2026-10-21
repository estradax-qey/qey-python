cmake_minimum_required(VERSION 3.18)
project(qey_python LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(Python 3.9 REQUIRED COMPONENTS Interpreter Development.Module
             OPTIONAL_COMPONENTS Development.SABIModule)
find_package(nanobind CONFIG REQUIRED)

# qey is built from source. Point QEY_SOURCE_DIR at a checkout to build against
# it; otherwise the release tag below is fetched.
set(QEY_TAG v0.1.0 CACHE STRING "The qey tag to build against")
if(QEY_SOURCE_DIR)
  set(qey_src ${QEY_SOURCE_DIR})
else()
  include(FetchContent)
  FetchContent_Declare(qey_core
    GIT_REPOSITORY https://github.com/estradax-qey/qey.git
    GIT_TAG ${QEY_TAG})
  FetchContent_Populate(qey_core)
  set(qey_src ${qey_core_SOURCE_DIR})
endif()

file(GLOB qey_sources ${qey_src}/qey/*.cc)
list(FILTER qey_sources EXCLUDE REGEX "_test\\.cc$")
add_library(qey_core STATIC ${qey_sources})
target_include_directories(qey_core PUBLIC ${qey_src})
find_package(Threads REQUIRED)
target_link_libraries(qey_core PUBLIC Threads::Threads)

nanobind_add_module(_qey STABLE_ABI NB_STATIC src/binding.cc)
target_link_libraries(_qey PRIVATE qey_core)
install(TARGETS _qey LIBRARY DESTINATION qey)
