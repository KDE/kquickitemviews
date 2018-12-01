CMAKE_MINIMUM_REQUIRED(VERSION 3.0)

PROJECT(kquickview)

IF(POLICY CMP0063)
   CMAKE_POLICY(SET CMP0063 NEW)
ENDIF(POLICY CMP0063)

FIND_PACKAGE(ECM 1.1.0 REQUIRED NO_MODULE)
LIST(APPEND CMAKE_MODULE_PATH "${ECM_MODULE_PATH}")

OPTION(BUILD_SHARED_LIBS "" OFF )

INCLUDE(ECMInstallIcons)
INCLUDE(ECMOptionalAddSubdirectory)

INCLUDE(KDEInstallDirs)
INCLUDE(KDECMakeSettings)
INCLUDE(KDECompilerSettings)

if(NOT BUILD_SHARED_LIBS)
    set(STATIC_LIBRARY 1)
else()
    set(STATIC_LIBRARY 0)
endif()

SET(CMAKE_AUTOMOC ON)
SET(CMAKE_AUTORCC ON)

SET(CMAKE_CXX_STANDARD 14)

# if(STATIC_LIBRARY)
    add_definitions(-DQT_PLUGIN)
    add_definitions(-DQT_STATICPLUGIN=1)
# else(STATIC_LIBRARY)
#     if (BUILD_TESTING)
#         add_subdirectory(autotests)
#     endif()
# endif(STATIC_LIBRARY)


FIND_PACKAGE(Qt5 CONFIG REQUIRED
    Core Gui Quick QuickControls2
)

add_definitions(-isystem ${Qt5Core_PRIVATE_INCLUDE_DIRS})

if("${CMAKE_BUILD_TYPE}" MATCHES "DEBUG")
    add_definitions(-DENABLE_EXTRA_VALIDATION=1)
endif()

SET(GENERIC_LIB_VERSION "1.0.0")

#File to compile
SET( kquickview_LIB_SRCS
    src/adapters/abstractitemadapter.cpp
    src/adapters/decorationadapter.cpp
    src/adapters/modeladapter.cpp
    src/adapters/scrollbaradapter.cpp
    src/adapters/selectionadapter.cpp
    src/flickablescrollbar.cpp
    src/plugin.cpp
    src/proxies/sizehintproxymodel.cpp
    src/singlemodelviewbase.cpp
    src/viewbase.cpp
    src/views/comboboxview.cpp
    src/views/flickable.cpp
    src/views/hierarchyview.cpp
    src/views/listview.cpp
    src/views/treeview.cpp
    src/viewport.cpp
    src/contextadapterfactory.cpp
    src/private/statetracker/index_p.cpp
    src/private/statetracker/geometry_p.cpp
    src/private/statetracker/proximity_p.cpp
    src/private/statetracker/model_p.cpp
    src/private/statetracker/modelitem_p.cpp
    src/private/statetracker/selection_p.cpp
    src/private/indexmetadata_p.cpp
    src/private/statetracker/content_p.cpp
    src/private/runtimetests_p.cpp
)

set(AUTOMOC_MOC_OPTIONS -Muri=org.kde.playground.kquickview)

add_library(kquickview STATIC ${kquickview_LIB_SRCS} )

target_link_libraries( kquickview
    Qt5::Core
    Qt5::Gui
    Qt5::Quick
    Qt5::QuickControls2
)

SET( kquickview_LIB_HDRS
    adapters/abstractitemadapter.h
    adapters/contextadapter.h
    adapters/decorationadapter.h
    adapters/modeladapter.h
    adapters/scrollbaradapter.h
    adapters/selectionadapter.h
    extensions/contextextension.h
    flickablescrollbar.h
    plugin.h
    proxies/sizehintproxymodel.h
    singlemodelviewbase.h
    contextadapterfactory.h
    viewbase.h
    views/comboboxview.h
    views/flickable.h
    views/hierarchyview.h
    views/listview.h
    views/treeview.h
    viewport.h
)

# Create include file aliases
foreach(header ${kquickview_LIB_HDRS})
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/src/${header}
        DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/api/KQuickView/
    )
endforeach()

target_include_directories(kquickview PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src;${CMAKE_CURRENT_BINARY_DIR}/api"
)

set_target_properties(kquickview
 PROPERTIES INCLUDE_DIRECTORIES
    "${CMAKE_CURRENT_BINARY_DIR}/api;${CMAKE_CURRENT_SOURCE_DIR}/src"
)

set_target_properties(kquickview PROPERTIES
    PUBLIC_HEADER
    "${CMAKE_CURRENT_BINARY_DIR}/api;${CMAKE_CURRENT_SOURCE_DIR}/src"
)

add_subdirectory(tests)

include_directories(${CMAKE_CURRENT_BINARY_DIR}/api/)
