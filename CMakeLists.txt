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
SET(CMAKE_AUTOUIC ON)

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

SET(GENERIC_LIB_VERSION "1.0.0")

#File to compile
SET( kquickview_LIB_SRCS
    src/plugin.cpp
    src/pixmapwrapper.cpp
    src/simpleflickable.cpp
    src/hierarchyview.cpp
    src/abstractquickview.cpp
    src/treetraversalreflector_p.cpp
    src/visiblerange.cpp
    src/abstractviewitem.cpp
    src/abstractselectableview.cpp
    src/abstractviewcompat.cpp
    src/quicktreeview.cpp
    src/quicklistview.cpp
    src/flickableview.cpp
    src/modelscrolladapter.cpp
    src/bindedcombobox.cpp
    src/symboliccolorizer.cpp
    src/flickablescrollbar.cpp
    src/sizehintproxymodel.cpp
    src/contextmanager.cpp
    src/modeladapter.cpp
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
    plugin.h
    pixmapwrapper.h
    simpleflickable.h
    hierarchyview.h
    abstractquickview.h
    treetraversalreflector_p.h
    visiblerange.h
    modeladapter.h
    abstractviewitem.h
    abstractselectableview.h
    abstractviewcompat.h
    quicktreeview.h
    quicklistview.h
    flickableview.h
    modelscrolladapter.h
    bindedcombobox.h
    symboliccolorizer.h
    flickablescrollbar.h
    sizehintproxymodel.h
    contextmanager.h
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

include_directories(${CMAKE_CURRENT_BINARY_DIR}/api/)
