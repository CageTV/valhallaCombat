# ValhallaCombat.dll on alandtse's CommonLibSSE-NG (Skyrim 1.5.97 to 1.6.1170 and 1.7.x, SE and AE); included by E:/WorkSpace/ng-build/CMakeLists.txt (see ng_plugin there).
find_path(VC_SIMPLEINI_INCLUDE_DIRS "SimpleIni.h")
ng_plugin(TARGET ValhallaCombat NAME ValhallaCombat VERSION 1.0.0 AUTHOR "D7ry and CageTV" ROOT "${CMAKE_CURRENT_LIST_DIR}"
    SOURCES src/bin/main.cpp src/bin/AI.cpp src/bin/attackHandler.cpp src/bin/balanceHandler.cpp src/bin/blockCommitment.cpp src/bin/blockHandler.cpp
            src/bin/data.cpp src/bin/debuffHandler.cpp src/bin/executionHandler.cpp src/bin/hitProcessor.cpp src/bin/Hooks.cpp src/bin/ModAPI.cpp
            src/bin/reactionHandler.cpp src/bin/settings.cpp src/bin/spiritHandler.cpp src/bin/staminaHandler.cpp src/bin/stunHandler.cpp
            src/bin/events/animEventHandler.cpp src/bin/events/cellLoadEventHandler.cpp src/bin/events/inputEventHandler.cpp src/include/Utils.cpp
    INCLUDES "${CMAKE_CURRENT_LIST_DIR}/ng" "${CMAKE_CURRENT_LIST_DIR}/src" "${CMAKE_CURRENT_LIST_DIR}/src/include" "${CMAKE_CURRENT_LIST_DIR}/src/include/lib" "${VC_SIMPLEINI_INCLUDE_DIRS}"
    DEFINES NOMINMAX WIN32_LEAN_AND_MEAN
    OPTIONS /utf-8 /wd4200 /WX-
    PCH "${CMAKE_CURRENT_LIST_DIR}/src/PCH.h")
