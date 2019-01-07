cmake_minimum_required(VERSION 3.0)

project(kquickitemviews)

if(POLICY CMP0063)
   CMAKE_POLICY(SET CMP0063 NEW)
endif(POLICY CMP0063)

find_package(ECM 1.1.0 REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH "${ECM_MODULE_PATH}")

option(BUILD_SHARED_LIBS "" OFF)

include(ECMInstallIcons)
include(ECMOptionalAddSubdirectory)
include(CMakePackageConfigHelpers)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings)

SET(CMAKE_C_VISIBILITY_PRESET hidden)
SET(CMAKE_CXX_VISIBILITY_PRESET hidden)
SET(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

if(NOT BUILD_SHARED_LIBS)
    set(ENABLE__STATIC_PLUGIN 1)
else()
    set(ENABLE__STATIC_PLUGIN 0)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 14)

if(ENABLE__STATIC_PLUGIN)
    add_definitions(-DQT_PLUGIN)
    add_definitions(-DQT_STATICPLUGIN=1)
    add_definitions(-DKQUICKITEMVIEWS_USE_STATIC_PLUGIN=1)
elseif (BUILD_TESTING)
    add_subdirectory(tests)
endif(ENABLE__STATIC_PLUGIN)

find_package(Qt5 CONFIG REQUIRED
    Core Gui Quick QuickControls2
)

add_definitions(-isystem ${Qt5Core_PRIVATE_INCLUDE_DIRS})

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/private/)
include_directories("${CMAKE_CURRENT_BINARY_DIR}/include")

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
    src/views/sizehintview.cpp
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

add_library(kquickitemviews ${kquickitemviews_LIB_SRCS} )

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
    SizeHintView
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

set(kquickitemviews_EXPORT_HDRS "")

# Create include file aliases
function(process_header prefix files)
    set(header_path ${CMAKE_CURRENT_BINARY_DIR}/include/KQuickItemViews/${prefix})
    set(tmp ${kquickitemviews_EXPORT_HDRS})

    foreach(header ${files})
        string(TOLOWER ${header} real_path)
        file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/src/${prefix}${real_path}.h
            DESTINATION ${header_path}
        )
        file(WRITE ${header_path}${header}
            "#include \"${real_path}.h\""
        )

        # Add to the `install` list
        list(APPEND tmp ${header_path}${real_path}.h)
        list(APPEND tmp ${header_path}${header})

        install(
            FILES
                ${header_path}${header}
                ${CMAKE_CURRENT_SOURCE_DIR}/src/${prefix}${real_path}.h
            DESTINATION
                ${CMAKE_INSTALL_FULL_INCLUDEDIR}/KQuickItemViews/${prefix}
            COMPONENT
                Headers
        )
    endforeach()

    set(kquickitemviews_EXPORT_HDRS ${tmp} PARENT_SCOPE)
endfunction()

process_header(adapters/ "${kquickitemviews_adapters_LIB_HDRS}")
process_header(views/ "${kquickitemviews_views_LIB_HDRS}")
process_header(strategies/ "${kquickitemviews_strategies_LIB_HDRS}")
process_header(extensions/ "${kquickitemviews_extensions_LIB_HDRS}")
process_header(proxies/ "${kquickitemviews_proxies_LIB_HDRS}")
process_header("" "${kquickitemviews_LIB_HDRS}")

# Configure the target config
set(kquickitemviews_CONFIG_PATH "${CMAKE_CURRENT_BINARY_DIR}/KQuickItemViewsConfig.cmake")

configure_package_config_file(
   "${CMAKE_CURRENT_SOURCE_DIR}/cmake/KQuickItemViewsConfig.cmake.in" ${kquickitemviews_CONFIG_PATH}
   INSTALL_DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}/cmake/KQuickItemViews/
   PATH_VARS INCLUDE_INSTALL_DIR
)

install( FILES ${kquickitemviews_CONFIG_PATH}
    DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}/cmake/KQuickItemViews/
    COMPONENT Devel
)

# Create the target
target_include_directories(kquickitemviews
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include/KQuickItemViews>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/
        ${CMAKE_CURRENT_SOURCE_DIR}/src/private/
        ${CMAKE_CURRENT_BINARY_DIR}/include
)

set_target_properties(kquickitemviews PROPERTIES
    PUBLIC_HEADER
    "${kquickitemviews_EXPORT_HDRS}"
)

export(TARGETS kquickitemviews
    FILE "${PROJECT_BINARY_DIR}/cmake/KQuickItemViewsTargets.cmake"
)

install(TARGETS kquickitemviews
    EXPORT KQuickItemViewsTargets
    LIBRARY DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}" COMPONENT kquickitemviews
    ARCHIVE DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
    COMPONENT Devel
)

install(EXPORT KQuickItemViewsTargets
    DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}/cmake/KQuickItemViews
    COMPONENT Devel
)

# Allow #include <KQuickItemViews/Foo> to work both both in add_subdirectory
# and on installed targets
install(CODE "execute_process( \
    COMMAND ${CMAKE_COMMAND} -E create_symlink \
        ${CMAKE_INSTALL_FULL_INCLUDEDIR}/KQuickItemViews \
        ${CMAKE_INSTALL_FULL_INCLUDEDIR}/KQuickItemViews/KQuickItemViews   \
    )"
)
