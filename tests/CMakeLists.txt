CMAKE_MINIMUM_REQUIRED(VERSION 3.1)
ADD_DEFINITIONS("-std=c++11")

PROJECT(modelviewtester)

SET(QT_MIN_VERSION  "5.7.0")
SET(KF5_DEP_VERSION "5.50.0" )
set(CMAKE_BUILD_TYPE DEBUG)

option(BUILD_COVERAGE ON)

IF(POLICY CMP0063)
    CMAKE_POLICY(SET CMP0063 NEW)
ENDIF(POLICY CMP0063)

find_package(Qt5 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
    Core
    Gui
    Quick
    QuickWidgets
    Widgets
    QuickControls2
)

# Kirigami is used for the menu and manual testing steps
find_package(KF5 "${KF5_DEP_VERSION}" REQUIRED COMPONENTS
    Kirigami2
)

INCLUDE_DIRECTORIES(SYSTEM ${Qt5Core_INCLUDES})

SET(modelviewtester_SRC
modelviewtester.cpp
freefloatingmodel.cpp
testmain.cpp
)

QT5_ADD_RESOURCES(modelviewtester_SRC
    modeltest.qrc
)

set(CMAKE_AUTOMOC ON)

ADD_EXECUTABLE( modelviewtester ${modelviewtester_SRC} )

TARGET_LINK_LIBRARIES( modelviewtester
    kquickitemviews
    KF5::Kirigami2
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Quick
)
