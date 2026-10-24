cmake_minimum_required(VERSION 3.21)

# ValhallaCombat.dll, the "legacy" build on CharmedBaryon's CommonLibSSE-NG: Skyrim SE 1.5.97 and AE before 1.6.1170. The current build (1.6.1170 and newer,
# including 1.7.x) comes from ../ng.cmake through E:/WorkSpace/ng-build. Same sources; VC_LEGACY_BUILD selects the few places where the two CommonLib trees differ.
project(ValhallaCombat VERSION 1.0.0 LANGUAGES CXX)

find_package(CommonLibSSE CONFIG REQUIRED)
find_path(VC_SIMPLEINI_INCLUDE_DIRS "SimpleIni.h")

set(R "${CMAKE_CURRENT_LIST_DIR}/..")
add_commonlibsse_plugin(${PROJECT_NAME} NAME "ValhallaCombat" AUTHOR "D7ry and CageTV" SOURCES
    ${R}/src/bin/main.cpp ${R}/src/bin/AI.cpp ${R}/src/bin/attackHandler.cpp ${R}/src/bin/balanceHandler.cpp ${R}/src/bin/blockCommitment.cpp ${R}/src/bin/blockHandler.cpp
    ${R}/src/bin/data.cpp ${R}/src/bin/debuffHandler.cpp ${R}/src/bin/executionHandler.cpp ${R}/src/bin/hitProcessor.cpp ${R}/src/bin/Hooks.cpp ${R}/src/bin/ModAPI.cpp
    ${R}/src/bin/reactionHandler.cpp ${R}/src/bin/settings.cpp ${R}/src/bin/spiritHandler.cpp ${R}/src/bin/staminaHandler.cpp ${R}/src/bin/stunHandler.cpp
    ${R}/src/bin/events/animEventHandler.cpp ${R}/src/bin/events/cellLoadEventHandler.cpp ${R}/src/bin/events/inputEventHandler.cpp ${R}/src/bin/menu.cpp
    ${R}/src/bin/hitstop.cpp ${R}/src/bin/parry.cpp ${R}/src/include/Utils.cpp)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_include_directories(${PROJECT_NAME} PRIVATE ${R}/ng ${R}/src ${R}/src/include ${R}/src/include/lib ${VC_SIMPLEINI_INCLUDE_DIRS})
target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN VC_LEGACY_BUILD)
target_precompile_headers(${PROJECT_NAME} PRIVATE ${R}/src/PCH.h)
target_compile_options(${PROJECT_NAME} PRIVATE /utf-8 /wd4200 /WX- $<$<CONFIG:Release>:/Z7>)
target_link_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Release>:/DEBUG /OPT:REF /OPT:ICF>)
