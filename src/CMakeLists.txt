set(QWINDOWKIT_GENERATED_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/../include)
set(QWINDOWKIT_BUILD_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/../etc/include)

set(QWINDOWKIT_ENABLED_TARGETS)
set(QWINDOWKIT_ENABLED_SUBDIRECTORIES)

# ----------------------------------
# Configurations
# ----------------------------------
set(_config_content "#ifndef QWKCONFIG_H\n#define QWKCONFIG_H\n\n")

if(QWINDOWKIT_FORCE_QT_WINDOW_CONTEXT)
    string(APPEND _config_content "#define QWINDOWKIT_FORCE_QT_WINDOW_CONTEXT 1\n")
else()
    string(APPEND _config_content "#define QWINDOWKIT_FORCE_QT_WINDOW_CONTEXT -1\n")
endif()

if(QWINDOWKIT_ENABLE_STYLE_AGENT)
    string(APPEND _config_content "#define QWINDOWKIT_ENABLE_STYLE_AGENT 1\n")
else()
    string(APPEND _config_content "#define QWINDOWKIT_ENABLE_STYLE_AGENT -1\n")
endif()

if(QWINDOWKIT_ENABLE_WINDOWS_SYSTEM_BORDERS)
    string(APPEND _config_content "#define QWINDOWKIT_ENABLE_WINDOWS_SYSTEM_BORDERS 1\n")
else()
    string(APPEND _config_content "#define QWINDOWKIT_ENABLE_WINDOWS_SYSTEM_BORDERS -1\n")
endif()

string(APPEND _config_content "\n#endif // QWKCONFIG_H\n")

file(WRITE ${QWINDOWKIT_BUILD_INCLUDE_DIR}/QWKCore/qwkconfig.h "${_config_content}")

if(QWINDOWKIT_INSTALL)
    install(FILES ${QWINDOWKIT_BUILD_INCLUDE_DIR}/QWKCore/qwkconfig.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${QWINDOWKIT_INSTALL_NAME}/QWKCore
    )
endif()

# ----------------------------------
# CMake API
# ----------------------------------
macro(qwk_add_library _target)
    set(options AUTOGEN NO_SYNC_INCLUDE NO_WIN_RC)
    set(oneValueArgs SYNC_INCLUDE_PREFIX PREFIX)
    set(multiValueArgs 
        SYNC_INCLUDE_OPTIONS
        SOURCES
        LINKS LINKS_PRIVATE
        QT_LINKS
        QT_INCLUDE_PRIVATE
        FEATURES
        INCLUDE_PRIVATE
    )
    cmake_parse_arguments(FUNC "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(FUNC_AUTOGEN)
        set(CMAKE_AUTOMOC ON)
        set(CMAKE_AUTOUIC ON)
        set(CMAKE_AUTORCC ON)
    endif()

    if(QWINDOWKIT_BUILD_STATIC)
        set(_type STATIC)
    else()
        set(_type SHARED)
    endif()

    add_library(${_target} ${_type})
    
    if(FUNC_SOURCES)
        target_sources(${_target} PRIVATE ${FUNC_SOURCES})
    endif()
    
    if(FUNC_LINKS)
        target_link_libraries(${_target} PUBLIC ${FUNC_LINKS})
    endif()
    
    if(FUNC_QT_LINKS)
        if(NOT QT_VERSION_MAJOR)
            find_package(QT NAMES Qt6 Qt5 COMPONENTS ${FUNC_QT_LINKS} REQUIRED)
        endif()
        find_package(Qt${QT_VERSION_MAJOR} COMPONENTS ${FUNC_QT_LINKS} REQUIRED)

        foreach(_qt_mod ${FUNC_QT_LINKS})
             target_link_libraries(${_target} PUBLIC Qt${QT_VERSION_MAJOR}::${_qt_mod})
        endforeach()
    endif()

    if(FUNC_QT_INCLUDE_PRIVATE)
        foreach(_qt_mod ${FUNC_QT_INCLUDE_PRIVATE})
            find_package(QT NAMES Qt6 Qt5 COMPONENTS ${_qt_mod} REQUIRED)
             if(TARGET Qt${QT_VERSION_MAJOR}::${_qt_mod}Private)
                 target_link_libraries(${_target} PRIVATE Qt${QT_VERSION_MAJOR}::${_qt_mod}Private)
             endif()
        endforeach()
    endif()
    
    if(FUNC_INCLUDE_PRIVATE)
        foreach(_inc_dir ${FUNC_INCLUDE_PRIVATE})
            if("${_inc_dir}" STREQUAL "*")
                 file(GLOB _all_files RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/*")
                 foreach(_file ${_all_files})
                     if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${_file}")
                         target_include_directories(${_target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/${_file}")
                     endif()
                 endforeach()
            else()
                target_include_directories(${_target} PRIVATE "${_inc_dir}")
            endif()
        endforeach()
    endif()
    
    if(FUNC_FEATURES)
        target_compile_features(${_target} PUBLIC ${FUNC_FEATURES})
    endif()

    if(FUNC_PREFIX)
        set(_prefix ${FUNC_PREFIX})
    else()
        string(TOUPPER ${_target} _prefix)
        string(MAKE_C_IDENTIFIER ${_prefix} _prefix)
    endif()

    if(QWINDOWKIT_BUILD_STATIC)
        target_compile_definitions(${_target} PUBLIC ${_prefix}_STATIC)
    endif()
    target_compile_definitions(${_target} PRIVATE ${_prefix}_LIBRARY)
    
    if(WIN32 OR APPLE)
        target_compile_definitions(${_target} PRIVATE QWINDOWKIT_LINUX_H)
    endif()

    # Add include directories
    target_include_directories(${_target} PRIVATE ${QWINDOWKIT_BUILD_INCLUDE_DIR})
    target_include_directories(${_target} PRIVATE .)

    # Library name
    if(${_target} MATCHES "^QWK(.+)")
        set(_name ${CMAKE_MATCH_1})
        set_target_properties(${_target} PROPERTIES EXPORT_NAME ${_name})
    else()
        set(_name ${_target})
    endif()

    add_library(${QWINDOWKIT_INSTALL_NAME}::${_name} ALIAS ${_target})

    if(FUNC_SYNC_INCLUDE_PREFIX)
        set(_inc_name ${FUNC_SYNC_INCLUDE_PREFIX})
    else()
        set(_inc_name ${_target})
    endif()

    set(_install_options)

    if(QWINDOWKIT_INSTALL)
        install(TARGETS ${_target}
            EXPORT ${QWINDOWKIT_INSTALL_NAME}Targets
            RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" OPTIONAL
            LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" OPTIONAL
            ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" OPTIONAL
        )

        target_include_directories(${_target} PUBLIC
            "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${QWINDOWKIT_INSTALL_NAME}>"
        )

        set(_install_options
            INSTALL_DIR "${CMAKE_INSTALL_INCLUDEDIR}/${QWINDOWKIT_INSTALL_NAME}/${_inc_name}"
        )
    endif()

    if(NOT FUNC_NO_SYNC_INCLUDE)
        set(_dest_dir "${QWINDOWKIT_GENERATED_INCLUDE_DIR}/${_inc_name}")
        file(MAKE_DIRECTORY "${_dest_dir}")
        file(MAKE_DIRECTORY "${_dest_dir}/private")
        
        file(GLOB_RECURSE _headers "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
        foreach(_header ${_headers})
             get_filename_component(_h_name ${_header} NAME)
             if(_h_name MATCHES "_p\\.h$")
                 configure_file("${_header}" "${_dest_dir}/private/${_h_name}" COPYONLY)
             else()
                 configure_file("${_header}" "${_dest_dir}/${_h_name}" COPYONLY)
             endif()
        endforeach()

        target_include_directories(${_target} PUBLIC
            "$<BUILD_INTERFACE:${QWINDOWKIT_GENERATED_INCLUDE_DIR}>"
        )
    endif()
endmacro()

# ----------------------------------
# Main Project
# ----------------------------------
add_subdirectory(core)

if(QWINDOWKIT_BUILD_WIDGETS)
    add_subdirectory(widgets)
endif()

if(QWINDOWKIT_BUILD_QUICK)
    add_subdirectory(quick)
endif()

# ----------------------------------
# Documentation
# ----------------------------------
if(QWINDOWKIT_BUILD_DOCUMENTATIONS)
    if(NOT DOXYGEN_EXECUTABLE)
        find_package(Doxygen REQUIRED)
    endif()

    set(_install_options)

if(QWINDOWKIT_INSTALL)
        set(_install_options INSTALL_DIR share/doc/${QWINDOWKIT_INSTALL_NAME})
    endif()

    set(_doc_targets ${QWINDOWKIT_ENABLED_TARGETS})

    set(QWINDOWKIT_DOXYGEN_TAGFILE
        ${CMAKE_BUILD_SHARE_DIR}/doc/${QWINDOWKIT_INSTALL_NAME}/${QWINDOWKIT_INSTALL_NAME}_tagfile.xml
        CACHE FILEPATH "QWindowKit doxygen tag file path" FORCE
    )

    qm_import(Doxygen)
    qm_setup_doxygen(QWindowKit_RunDoxygen
        NAME "QWindowKit"
        DESCRIPTION "${QWINDOWKIT_DESCRIPTION}"
        MDFILE ../README.md
        OUTPUT_DIR ${CMAKE_BUILD_SHARE_DIR}/doc/${QWINDOWKIT_INSTALL_NAME}
        INPUT ${QWINDOWKIT_ENABLED_SUBDIRECTORIES}
        TARGETS ${_doc_targets}
        DEPENDS ${_doc_targets}
        NO_EXPAND_MACROS Q_OBJECT Q_GADGET Q_DECLARE_TR_FUNCTIONS
        COMPILE_DEFINITIONS Q_SIGNALS=Q_SIGNALS Q_SLOTS=Q_SLOTS
        GENERATE_TAGFILE "${QWINDOWKIT_DOXYGEN_TAGFILE}"
        ${_install_options}
    )
endif()

# ----------------------------------
# Install
# ----------------------------------
if(QWINDOWKIT_INSTALL)
    qm_basic_install(
        NAME ${QWINDOWKIT_INSTALL_NAME}
        VERSION ${QWINDOWKIT_VERSION}
        INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/${QWINDOWKIT_INSTALL_NAME}
        CONFIG_TEMPLATE "${QWINDOWKIT_INSTALL_NAME}Config.cmake.in"
        NAMESPACE ${QWINDOWKIT_INSTALL_NAME}::
        EXPORT ${QWINDOWKIT_INSTALL_NAME}Targets
        WRITE_CONFIG_OPTIONS NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

    # Install shared files
    include("../share/install.cmake")
endif()