#Actualización 2024
cmake_minimum_required (VERSION 3.16)
project (gptbon)
set(CMAKE_PROJECT_VERSION 1.0)

# User-defined build flags
option(ENABLE_IBA "Enable IBA support" OFF)
option(ENABLE_GPU "Enable GPU support" OFF)
option(ENABLE_REDIS "Enable REDIS support" OFF)
option(ENABLE_INFLUX "Enable Influx support" OFF)
option(ENABLE_POWERCOLLECTOR "Enable energy measurements" OFF)
option(ENABLE_IOCOLLECTOR "Enable I/O measurements" OFF)
option(ENABLE_NETWORKCOLLECTOR "Enable eth and wlan network measurements" OFF)
option(ENABLE_PI "Enable Raspberry PI measurements" OFF)
option(ENABLE_AMD "Enable AMD counter measurements" OFF)

# User-defined paths
set(HIREDIS_PATH "" CACHE PATH "Path to the Hiredis library directory")
set(LOCAL_INCLUDE_DIR "" CACHE PATH "Optional path to local include directory")

# Paths
set(tool_dest "bin/")
set(include_dest "include/")
set(main_lib_dest "lib/")

# CMAKE OPTIONS
#message(${CMAKE_SOURCE_DIR})
set(CMAKE_C++_COMPILER g++)
set(CMAKE_CXX_FLAGS "-std=c++2a -D_GNU_SOURCE -g -ldl -pthread")
if(ENABLE_IBA)
    message(STATUS "Building with IBA support")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -libverbs ")
else ()
    message(STATUS "Building without IBA support")
endif ()

if(ENABLE_GPU)
    message(STATUS "Building with GPU support")
    find_package(CUDA REQUIRED)
    #message(${CUDA_INCLUDE_DIRS})
    include_directories(${CUDA_INCLUDE_DIRS})
    include_directories(${CUDA_TOOLDKIT_ROOT_DIR})
    find_path(LIBDL_INCLUDE_DIR dlfcn.h)
else ()
    message(STATUS "Building without GPU support")
endif ()

if(ENABLE_REDIS)
    message(STATUS "Building with REDIS support")
    ## Redis sources check
    if(NOT DEFINED HIREDIS_PATH OR "${HIREDIS_PATH}" STREQUAL "")
        message(FATAL_ERROR "ENABLE_REDIS is ON, but HIREDIS_PATH is not set. Please specify -DHIREDIS_PATH=/path/to/hiredis")
    endif()

    # Search for library in HIREDIS_PATH, HIREDIS_PATH/lib, or HIREDIS_PATH/lib64
    find_library(HIREDIS_LIBRARY hiredis HINTS ${HIREDIS_PATH} ${HIREDIS_PATH}/lib ${HIREDIS_PATH}/lib64)
    
    # Search for header parent directory in HIREDIS_PATH or HIREDIS_PATH/include
    find_path(HIREDIS_INCLUDE_DIR hiredis/hiredis.h HINTS ${HIREDIS_PATH} ${HIREDIS_PATH}/include)

    # Check if hiredis library is found
    if(NOT HIREDIS_LIBRARY)
        message(FATAL_ERROR "Hiredis library not found in ${HIREDIS_PATH}")
    else()
        message(STATUS "Hiredis library found: ${HIREDIS_LIBRARY}")
    endif()

    # Check if hiredis header directory is found
    if(NOT HIREDIS_INCLUDE_DIR)
        message(FATAL_ERROR "Hiredis header (hiredis/hiredis.h) not found in ${HIREDIS_PATH}")
    else()
        message(STATUS "Hiredis include directory found: ${HIREDIS_INCLUDE_DIR}")
        include_directories(${HIREDIS_INCLUDE_DIR})
    endif()
else ()
    message(STATUS "Building without REDIS support")
endif ()

if(ENABLE_INFLUX)
    message(STATUS "Building with Influx support")
else ()
    message(STATUS "Building without Influx support")
endif ()

if(ENABLE_POWERCOLLECTOR)
    message(STATUS "Building with power and energy data collection")
else ()
    message(STATUS "Building without power and energy data collection")
endif ()

if(ENABLE_IOCOLLECTOR)
    message(STATUS "Building with I/O data collection")
else ()
    message(STATUS "Building without I/O data collection")
endif ()

if(ENABLE_NETWORKCOLLECTOR)
    message(STATUS "Building with network data collection")
else ()
    message(STATUS "Building without network data collection")
endif ()

if(ENABLE_PI)
	message(STATUS "Building with Raspi support")
else ()
	message(STATUS "Building without Raspi support")
endif ()

if(ENABLE_AMD)
	message(STATUS "Building with AMD counter collection support")
else ()
	message(STATUS "Building without AMD counter collection support")
endif ()



## LIBRARY ##
add_library(${PROJECT_NAME} STATIC)

if(ENABLE_IBA)
    target_sources(${PROJECT_NAME}
            PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/src/common.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/src/cpu_info.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/src/devices_info.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/src/dir_path.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/src/memory_info.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/src/net_info.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/src/Packed_sample.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/src/power_cpu_info.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/src/sample_info.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/src/system_features.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/src/temp_info.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/src/network_data.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/src/server_data.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/src/servidor_monitor.cpp
            ##${CMAKE_CURRENT_SOURCE_DIR}/lib_src/src/redisdb.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/src/iba_info.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/src/cliente_monitor_iba.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/src/cliente_monitor.cpp

    )
    if(ENABLE_REDIS)
        target_sources(${PROJECT_NAME}
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/src/redisdb.cpp
    )
    endif ()
    if(ENABLE_GPU)
        target_sources(${PROJECT_NAME}
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/src/gpu_info.cpp
    )
    endif ()
else ()
    target_sources(${PROJECT_NAME}
            PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/src/cliente_monitor.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/src/common.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/src/cpu_info.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/src/devices_info.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/src/dir_path.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/src/memory_info.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/src/net_info.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/src/Packed_sample.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/src/power_cpu_info.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/src/sample_info.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/src/system_features.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/src/temp_info.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/src/network_data.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/src/server_data.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/src/servidor_monitor.cpp
            #${CMAKE_CURRENT_SOURCE_DIR}/lib_src/src/redisdb.cpp
    )
    if(ENABLE_REDIS)
        target_sources(${PROJECT_NAME}
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/src/redisdb.cpp
    )
    endif ()
    if(ENABLE_GPU)
        target_sources(${PROJECT_NAME}
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/src/gpu_info.cpp
    )
    endif ()
endif ()

target_include_directories(${PROJECT_NAME}
        #PRIVATE
        # where the library itself will look for its internal headers
        #${CMAKE_CURRENT_SOURCE_DIR}/priv-include
        PUBLIC
        # where top-level project will look for the library's public headers
	    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib_src/include>
	    #$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/include>
        # where external projects will look for the library's public headers
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/lib_src>
        )

        # Build-option macros must be defined on the library target too.
# PUBLIC makes them visible both while compiling gptbon and to executables/headers that consume it.
target_compile_definitions(${PROJECT_NAME}
        PUBLIC
        ENABLE_IBA=$<BOOL:${ENABLE_IBA}>
        ENABLE_GPU=$<BOOL:${ENABLE_GPU}>
        ENABLE_REDIS=$<BOOL:${ENABLE_REDIS}>
        ENABLE_INFLUX=$<BOOL:${ENABLE_INFLUX}>
        ENABLE_POWERCOLLECTOR=$<BOOL:${ENABLE_POWERCOLLECTOR}>
        ENABLE_IOCOLLECTOR=$<BOOL:${ENABLE_IOCOLLECTOR}>
        ENABLE_NETWORKCOLLECTOR=$<BOOL:${ENABLE_NETWORKCOLLECTOR}>
        ENABLE_PI=$<BOOL:${ENABLE_PI}>
        ENABLE_AMD=$<BOOL:${ENABLE_AMD}>
        )

# Dependencies used by optional code should belong to the library interface.
# That way every executable linking gptbon gets the same requirements.
if(ENABLE_IBA)
    target_link_libraries(${PROJECT_NAME} PUBLIC -libverbs)
endif()

if(ENABLE_REDIS)
    target_link_libraries(${PROJECT_NAME} PUBLIC ${HIREDIS_LIBRARY})
endif()

if(ENABLE_GPU)
    target_include_directories(${PROJECT_NAME} PUBLIC ${CUDA_INCLUDE_DIRS})
endif()


# without its public headers won't get INSTALLED
if(ENABLE_IBA)
    set(public_headers
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/include/common.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/include/cpu_info.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/include/devices_info.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/include/dir_path.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/include/memory_info.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/include/net_info.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/include/network_data.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/include/Packed_sample.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/include/power_cpu_info.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/include/system_features.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/include/temp_info.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/include/network_data.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/include/server_data.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/include/servidor_monitor.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/include/redisdb.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/include/iba_info.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/include/cliente_monitor_iba.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/include/cliente_monitor.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/include/gpu_info.hpp
    )
else ()
    set(public_headers
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/include/cliente_monitor.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/include/common.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/include/cpu_info.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/include/devices_info.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/include/dir_path.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/include/memory_info.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/include/net_info.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/include/network_data.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/include/Packed_sample.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/include/power_cpu_info.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/include/system_features.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/include/temp_info.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/include/network_data.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/include/server_data.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/include/servidor_monitor.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/include/redisdb.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lib_src/include/gpu_info.hpp
    )
endif ()


# note that ${public_headers} has to be in quotes
set_target_properties(${PROJECT_NAME} PROPERTIES PUBLIC_HEADER "${public_headers}")

# note that it is not CMAKE_INSTALL_PREFIX we are checking here
if(DEFINED CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    message(
            STATUS
            "CMAKE_INSTALL_PREFIX is not set\n"
            "Default value: ${CMAKE_INSTALL_PREFIX}\n"
            "Will set it to ${CMAKE_SOURCE_DIR}/install"
    )
    set(CMAKE_INSTALL_PREFIX
            "${CMAKE_SOURCE_DIR}/install"
            CACHE PATH "Where the library will be installed to" FORCE
    )
else()
    message(
            STATUS
            "CMAKE_INSTALL_PREFIX was already set\n"
            "Current value: ${CMAKE_INSTALL_PREFIX}"
    )
endif()

## Threads sources
find_package (Threads)

# InfluxDB sources
#find_package(InfluxDB)

include_directories("include/")
# In case there are dependencies installed locally
if(DEFINED LOCAL_INCLUDE_DIR AND NOT "${LOCAL_INCLUDE_DIR}" STREQUAL "")
    message(STATUS "Adding local include directory: ${LOCAL_INCLUDE_DIR}")
    include_directories("${LOCAL_INCLUDE_DIR}")
endif()

# DAEMON: Original
add_executable(proc-sender src/daemon_monitor.cpp)
target_link_libraries(proc-sender PUBLIC gptbon)

# TBON generic sender
add_executable(generic-sender src/test_daemon.cpp)
target_link_libraries(generic-sender PUBLIC gptbon)

# SERVER:
add_executable(LDS src/test_server.cpp)
#target_link_libraries(LDS PUBLIC gptbon InfluxData::InfluxDB)
target_link_libraries(LDS PUBLIC gptbon)

#[[ CODE SIMPLIFIED WITH FLAGS CONSTUCTIONS. THIS MAY BE OBSOLETE 
# DAEMON: Original
add_executable(proc-sender src/daemon_monitor.cpp)
if(ENABLE_GPU)
    target_compile_definitions(proc-sender PRIVATE ENABLE_GPU=1)
else ()
    target_compile_definitions(proc-sender PRIVATE ENABLE_GPU=0)
endif ()
if(ENABLE_REDIS)
    target_compile_definitions(proc-sender PRIVATE ENABLE_REDIS=1)
else ()
    target_compile_definitions(proc-sender PRIVATE ENABLE_REDIS=0)
endif ()
if(ENABLE_INFLUX)
	target_compile_definitions(proc-sender PRIVATE ENABLE_INFLUX=1)
else ()
	target_compile_definitions(proc-sender PRIVATE ENABLE_INFLUX=0)
endif ()
if(ENABLE_IBA)
    target_compile_definitions(proc-sender PRIVATE ENABLE_IBA=1)
    target_link_libraries(proc-sender PUBLIC gptbon -libverbs)
else ()
    target_compile_definitions(proc-sender PRIVATE ENABLE_IBA=0)
    target_link_libraries(proc-sender PUBLIC gptbon)
endif ()
if(ENABLE_REDIS)
    target_link_libraries(proc-sender PUBLIC ${HIREDIS_LIBRARY})
endif ()
if(ENABLE_POWERCOLLECTOR)
    target_compile_definitions(proc-sender PRIVATE ENABLE_POWERCOLLECTOR=1)
else ()
    target_compile_definitions(proc-sender PRIVATE ENABLE_POWERCOLLECTOR=0)
endif()
if(ENABLE_IOCOLLECTOR)
    target_compile_definitions(proc-sender PRIVATE ENABLE_IOCOLLECTOR=1)
else ()
    target_compile_definitions(proc-sender PRIVATE ENABLE_IOCOLLECTOR=0)
endif()
if(ENABLE_NETWORKCOLLECTOR)
    target_compile_definitions(proc-sender PRIVATE ENABLE_NETWORKCOLLECTOR=1)
else ()
    target_compile_definitions(proc-sender PRIVATE ENABLE_NETWORKCOLLECTOR=0)
endif()
if(ENABLE_PI)
	target_compile_definitions(proc-sender PRIVATE ENABLE_PI=1)
else ()
	target_compile_definitions(proc-sender PRIVATE ENABLE_PI=0)
endif ()
if(ENABLE_AMD)
	target_compile_definitions(proc-sender PRIVATE ENABLE_AMD=1)
else ()
	target_compile_definitions(proc-sender PRIVATE ENABLE_AMD=0)
endif ()

# TBON generic sender
add_executable(generic-sender src/test_daemon.cpp)
if(ENABLE_IBA)
    target_link_libraries(generic-sender PUBLIC gptbon -libverbs)
else ()
    target_link_libraries(generic-sender PUBLIC gptbon)
endif ()

# SERVER:
add_executable(LDS src/test_server.cpp)
#target_link_libraries(LDS PUBLIC gptbon InfluxData::InfluxDB)
target_link_libraries(LDS PUBLIC gptbon )
if(ENABLE_REDIS)
    target_link_libraries(LDS PUBLIC ${HIREDIS_LIBRARY})
endif ()
#]]

# Install
install(TARGETS gptbon DESTINATION "${main_lib_dest}")
install(FILES ${public_headers} DESTINATION "${include_dest}")
install(TARGETS proc-sender DESTINATION "${tool_dest}")
install(TARGETS LDS DESTINATION "${tool_dest}")
