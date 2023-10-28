cmake_minimum_required(VERSION 3.24)
project(ocs-runner C CXX)

set(CMAKE_CXX_LINK_GROUP_USING_NO_AS_NEEDED_SUPPORTED TRUE)
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
        AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CMAKE_CXX_LINK_GROUP_USING_NO_AS_NEEDED "LINKER:--no-as-needed"
            "LINKER:--as-needed")
else ()
    set(CMAKE_CXX_LINK_GROUP_USING_NO_AS_NEEDED
            "" "")
endif ()

find_package(PkgConfig REQUIRED)
pkg_check_modules(SPDK REQUIRED spdk_nvme spdk_env_dpdk)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

add_executable(ocs-runner runner.cc)
target_include_directories(ocs-runner PUBLIC ${SPDK_INCLUDE_DIRS})
target_link_libraries(ocs-runner PUBLIC
        $<LINK_GROUP:NO_AS_NEEDED,${SPDK_LINK_LIBRARIES}>)
target_link_libraries(ocs-runner PUBLIC Threads::Threads)