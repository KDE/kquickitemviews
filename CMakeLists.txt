CMAKE_MINIMUM_REQUIRED(VERSION 3.0)

PROJECT(libqmlwidgets)

IF(POLICY CMP0063)
   CMAKE_POLICY(SET CMP0063 NEW)
ENDIF(POLICY CMP0063)

FIND_PACKAGE(ECM 1.1.0 REQUIRED NO_MODULE)
LIST(APPEND CMAKE_MODULE_PATH "${ECM_MODULE_PATH}")

INCLUDE(ECMInstallIcons)
INCLUDE(ECMOptionalAddSubdirectory)

INCLUDE(KDEInstallDirs)
INCLUDE(KDECMakeSettings)
INCLUDE(KDECompilerSettings)

SET(CMAKE_AUTOMOC ON)
SET(CMAKE_AUTORCC ON)
SET(CMAKE_AUTOUIC ON)

SET(CMAKE_CXX_STANDARD 14)

FIND_PACKAGE(Qt5 CONFIG REQUIRED
    Core
    Gui

    Quick
    QuickWidgets
    QuickControls2
)

INCLUDE_DIRECTORIES(SYSTEM ${Qt5Widgets_INCLUDES} ${Qt5Core_INCLUDES})

ADD_DEFINITIONS(${Qt5Core_DEFINITIONS})

SET(GENERIC_LIB_VERSION "1.0.0")

#File to compile
SET( libqmlwidgets_LIB_SRCS
    plugin.cpp
    pixmapwrapper.cpp
    simpleflickable.cpp
    hierarchyview.cpp
    abstractquickview.cpp
    treetraversalreflector_p.cpp
    treetraversalrange_p.cpp
    abstractviewitem.cpp
    abstractselectableview.cpp
    abstractviewcompat.cpp
    quicktreeview.cpp
    quicklistview.cpp
    flickableview.cpp
    modelscrolladapter.cpp
    bindedcombobox.cpp
    symboliccolorizer.cpp
    flickablescrollbar.cpp
)

ADD_LIBRARY( libqmlwidgets  STATIC ${libqmlwidgets_LIB_SRCS} )

target_link_libraries( libqmlwidgets
    Qt5::Core
    Qt5::Gui
    Qt5::Quick
    Qt5::QuickWidgets
    Qt5::QuickControls2
)

SET( libqmlwidgets_LIB_HDRS
    bubble.h
    plugin.h
    messagebuilder.h
    pixmapwrapper.h
)
