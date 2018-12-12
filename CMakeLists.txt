cmake_minimum_required(VERSION 3.0)

project(kquickitemviews)

if(POLICY CMP0063)
   CMAKE_POLICY(SET CMP0063 NEW)
endif(POLICY CMP0063)

find_package(ECM 1.1.0 REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH "${ECM_MODULE_PATH}")

option(BUILD_SHARED_LIBS "" OFF )

include(ECMInstallIcons)
include(ECMOptionalAddSubdirectory)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings)

if(NOT BUILD_SHARED_LIBS)
    set(STATIC_LIBRARY 1)
else()
    set(STATIC_LIBRARY 0)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 14)

if(STATIC_LIBRARY)
    add_definitions(-DQT_PLUGIN)
    add_definitions(-DQT_STATICPLUGIN=1)
else(STATIC_LIBRARY)
    if (BUILD_TESTING)
        add_subdirectory(tests)
    endif()
endif(STATIC_LIBRARY)

find_package(Qt5 CONFIG REQUIRED
    Core Gui Quick QuickControls2
)

add_definitions(-isystem ${Qt5Core_PRIVATE_INCLUDE_DIRS})

if("${CMAKE_BUILD_TYPE}" MATCHES "DEBUG")
    add_definitions(-DENABLE_EXTRA_VALIDATION=1)
endif()

set(GENERIC_LIB_VERSION "1.0.0")

#File to compile
set( kquickitemviews_LIB_SRCS

    # Adapters
    src/adapters/abstractitemadapter.cpp
    src/adapters/decorationadapter.cpp
    src/adapters/modeladapter.cpp
    src/adapters/scrollbaradapter.cpp
    src/adapters/selectionadapter.cpp
    src/adapters/geometryadapter.cpp

    # Building blocks
    src/flickablescrollbar.cpp
    src/plugin.cpp
    src/proxies/sizehintproxymodel.cpp
    src/singlemodelviewbase.cpp
    src/viewbase.cpp
    src/viewport.cpp
    src/contextadapterfactory.cpp
    src/qmodelindexwatcher.cpp
    src/qmodelindexbinder.cpp
    src/delegatechoice.cpp
    src/delegatechooser.cpp

    # Views
    src/views/comboboxview.cpp
    src/views/flickable.cpp
    src/views/hierarchyview.cpp
    src/views/listview.cpp
    src/views/treeview.cpp
    src/views/indexview.cpp

    # State trackers
    src/private/statetracker/index_p.cpp
    src/private/statetracker/geometry_p.cpp
    src/private/statetracker/proximity_p.cpp
    src/private/statetracker/model_p.cpp
    src/private/statetracker/modelitem_p.cpp
    src/private/statetracker/selection_p.cpp
    src/private/statetracker/content_p.cpp
    src/private/statetracker/continuity_p.cpp

    src/private/runtimetests_p.cpp
    src/private/indexmetadata_p.cpp
    src/private/geostrategyselector_p.cpp

    # Geometry strategies
    src/strategies/justintime.cpp
    src/strategies/proxy.cpp
    src/strategies/role.cpp
    src/strategies/delegate.cpp
    src/strategies/uniform.cpp
    src/strategies/aheadoftime.cpp
)

set(AUTOMOC_MOC_OPTIONS -Muri=org.kde.playground.kquickitemviews)

add_library(kquickitemviews STATIC ${kquickitemviews_LIB_SRCS} )

target_link_libraries( kquickitemviews
    Qt5::Core
    Qt5::Gui
    Qt5::Quick
    Qt5::QuickControls2
)

set( kquickitemviews_adapters_LIB_HDRS
    AbstractItemAdapter
    ContextAdapter
    DecorationAdapter
    ModelAdapter
    ScrollbarAdapter
    SelectionAdapter
    GeometryAdapter
)

set( kquickitemviews_views_LIB_HDRS
    ComboboxView
    Flickable
    IndexView
    HierarchyView
    ListView
    TreeView
)

set( kquickitemviews_strategies_LIB_HDRS
    JustInTime
    Proxy
    Role
    Delegate
    AheadOfTime
    Uniform
)

set( kquickitemviews_extensions_LIB_HDRS
    ContextExtension
)

set( kquickitemviews_proxies_LIB_HDRS
    SizeHintProxyModel
)

set( kquickitemviews_LIB_HDRS
    FlickableScrollbar
    Plugin
    SingleModelViewBase
    ContextAdapterFactory
    QModelIndexWatcher
    QModelIndexBinder
    ViewBase
    Viewport
    DelegateChoice
    DelegateChooser
)

# Create include file aliases
function(process_header prefix files)
    foreach(header ${files})
        string(TOLOWER ${header} real_path)
        file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/src/${prefix}${real_path}.h
            DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/include/KQuickItemViews/${prefix}
        )
        file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/include/KQuickItemViews/${prefix}${header}
            "#include \"${real_path}.h\""
        )
    endforeach()
endfunction()

process_header(adapters/ "${kquickitemviews_adapters_LIB_HDRS}")
process_header(views/ "${kquickitemviews_views_LIB_HDRS}")
process_header(strategies/ "${kquickitemviews_strategies_LIB_HDRS}")
process_header(extensions/ "${kquickitemviews_extensions_LIB_HDRS}")
process_header(proxies/ "${kquickitemviews_proxies_LIB_HDRS}")
process_header("" "${kquickitemviews_LIB_HDRS}")

target_include_directories(kquickitemviews PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:include/KQuickItemViews>
)

set_target_properties(kquickitemviews
 PROPERTIES INCLUDE_DIRECTORIES
    "${CMAKE_CURRENT_SOURCE_DIR}/src;${CMAKE_CURRENT_SOURCE_DIR}/include/"
)

set_target_properties(kquickitemviews PROPERTIES
    PUBLIC_HEADER
    "${CMAKE_CURRENT_BINARY_DIR}/include/"
)

include_directories(${CMAKE_CURRENT_BINARY_DIR}/src/private/)
include_directories("${CMAKE_CURRENT_BINARY_DIR}/include")
