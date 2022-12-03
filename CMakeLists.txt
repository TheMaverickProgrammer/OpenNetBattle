cmake_minimum_required(VERSION 3.12)

project(BattleNetwork-Engine)
set(CMAKE_CXX_STANDARD          17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

option(ONB_HTTPS_SUPPORT "TURN ON AFTER INSTALLING OPENSSL FOR POCO. Please read: https://docs.pocoproject.org/current/00200-GettingStarted.html" OFF)

include(FindLua)

execute_process(COMMAND git submodule update --init -- extern/Swoosh
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

execute_process(COMMAND git submodule update --init -- extern/SFML
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Config.cmake)

find_package(Poco REQUIRED Foundation Net JSON)
find_package(Threads REQUIRED)
find_package(Lua REQUIRED)
find_package(FluidSynth REQUIRED)
# find_package(sol2 REQUIRED) # commented out until sol2 vcpkg is up to date

include_directories(extern/Swoosh/src)
include_directories(BattleNetwork/sfMidi/include)
include_directories(BattleNetwork/sfMidi/src)
include_directories(BattleNetwork/sol/include) # temporary sol2 bugfix
add_subdirectory(extern/SFML)

file(GLOB_RECURSE bnFiles CONFIGURE_DEPENDS
        "BattleNetwork/*.h"
        "BattleNetwork/*.cpp"
	"BattleNetwork/renderers/*.h"
	"BattleNetwork/renderers/*.cpp"
	"BattleNetwork/netplay/*.h"
	"BattleNetwork/netplay/*.cpp"
	"BattleNetwork/overworld/*.h"
	"BattleNetwork/overworld/*.cpp"
	"BattleNetwork/widgets/*.h"
	"BattleNetwork/widgets/*.cpp"
        "BattleNetwork/bindings/*.h"
        "BattleNetwork/bindings/*.cpp"
        "BattleNetwork/stx/*.h"
        "BattleNetwork/stx/*.cpp"
        "BattleNetwork/zip/*.h"
        "BattleNetwork/zip/*.c"
        "BattleNetwork/crypto/*.h"
        "BattleNetwork/sago/*.h"
        "BattleNetwork/sago/*.cpp"
        "BattleNetwork/mmbn.ico.c"
        )

list(FILTER bnFiles EXCLUDE REGEX "BattleNetwork/neurolink/navi_personality_draft_5_memories.cpp")

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/AddFiles.cmake)

# ScriptResourceManager has sol2 templates that generate massive number of obj sections...
# further reading: https://docs.microsoft.com/en-us/cpp/build/reference/bigobj-increase-number-of-sections-in-dot-obj-file?view=msvc-160
if(MSVC)
	add_compile_options(/bigobj)
endif()

add_executable(BattleNetwork BattleNetwork/main.cpp ${bnFiles} ${addBNFiles})
add_compile_definitions(NOMINMAX)

option(ONB_MOD_SUPPORT "This tells BN engine to define a ScriptResourceMananger and add sol2 bindings. WARNING: sol2 bindings increase compile times by 2-3x!" ON)
if(ONB_MOD_SUPPORT) 
        add_definitions(-DONB_MOD_SUPPORT=1)
        MESSAGE(STATUS "Added ONB mod support")
else()
        add_definitions(-DONB_MOD_SUPPORT=0)
        MESSAGE(STATUS "Disabled ONB mod support")
endif()

# if building for windows, make sure Poco ENABLE_NETSSL_WIN and ENABLE_NETSSL are both ON!!!
if(ONB_HTTPS_SUPPORT) 
        add_definitions(-DONB_HTTPS_SUPPORT=1)
        find_package(OpenSSL REQUIRED)
	if(MSVC)
        	find_package(Poco REQUIRED NetSSLWin Crypto XML)
        	MESSAGE(STATUS "Added Poco NetSSLWin support for ONB https requests")
	else()
        	find_package(Poco REQUIRED NetSSL Crypto XML)
        	MESSAGE(STATUS "Added Poco NetSSL and OpenSSL support for ONB https requests")
	endif()
else()
        add_definitions(-DONB_HTTPS_SUPPORT=0)
endif()

source_group(TREE ${PROJECT_SOURCE_DIR} FILES ${bnFiles})
source_group(TREE ${PROJECT_SOURCE_DIR} FILES ${addBNFiles})

target_compile_definitions(BattleNetwork PRIVATE SOL_ALL_SAFETIES_ON)

target_include_directories(BattleNetwork PRIVATE ${LUA_INCLUDE_DIR})
target_link_libraries(BattleNetwork sfml-graphics sfml-audio sfml-network sfml-system sfml-window)
target_link_libraries(BattleNetwork ${FLUIDSYNTH_LIBRARIES})

target_link_libraries(BattleNetwork Poco::Net Poco::Foundation Poco::JSON)

if(ONB_HTTPS_SUPPORT)
        target_link_libraries(BattleNetwork OpenSSL::SSL)

	if(MSVC)
        	target_link_libraries(BattleNetwork Poco::NetSSLWin Poco::Crypto)
	else()
        	target_link_libraries(BattleNetwork Poco::NetSSL Poco::Crypto)
	endif()
endif()

target_link_libraries(BattleNetwork Threads::Threads)
target_link_libraries(BattleNetwork ${LUA_LIBRARIES})
# target_link_libraries(BattleNetwork sol2::sol2)

set_target_properties(BattleNetwork
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build/$<CONFIG>"
)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Compiler.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/PostBuild.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/GitInfo.cmake)