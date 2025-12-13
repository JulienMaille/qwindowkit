# Native CMake equivalents of the build helpers used by the main branch.
set(QWINDOWKIT_QT_COMPONENTS Core Gui)
if(QWINDOWKIT_BUILD_WIDGETS)
    list(APPEND QWINDOWKIT_QT_COMPONENTS Widgets)
endif()
if(QWINDOWKIT_BUILD_QUICK)
    list(APPEND QWINDOWKIT_QT_COMPONENTS Quick)
endif()
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS ${QWINDOWKIT_QT_COMPONENTS})
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS ${QWINDOWKIT_QT_COMPONENTS})
set(QWINDOWKIT_QT_PRIVATE_COMPONENTS)
foreach(_module IN LISTS QWINDOWKIT_QT_COMPONENTS)
    list(APPEND QWINDOWKIT_QT_PRIVATE_COMPONENTS ${_module}Private)
endforeach()

macro(qwk_init_buildsystem _prefix)
    set(QWINDOWKIT_GENERATED_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/../include)
    set(QWINDOWKIT_BUILD_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/../etc/include)
    set(QWINDOWKIT_BUILD_DATA_DIR ${CMAKE_CURRENT_BINARY_DIR}/../share)
    set(QWINDOWKIT_BUILD_DOC_DIR ${CMAKE_CURRENT_BINARY_DIR}/../doc)
    set(QWINDOWKIT_INSTALL_DOC_DIR ${CMAKE_INSTALL_DATADIR}/doc/${QWINDOWKIT_INSTALL_NAME})
    set(_qwk_config_content "#ifndef QWKCONFIG_H\n#define QWKCONFIG_H\n\n")
endmacro()

macro(qwk_add_definition _name)
    cmake_parse_arguments(_definition "" "CONDITION" "" ${ARGN})
    if(${_definition_CONDITION})
        string(APPEND _qwk_config_content "#define ${_name} 1\n")
    else()
        string(APPEND _qwk_config_content "#define ${_name} -1\n")
    endif()
endmacro()

function(qwk_configure_target _target)
    cmake_parse_arguments(_target_args "" "" "SOURCES;LINKS;LINKS_PRIVATE;QT_LINKS;QT_INCLUDE_PRIVATE;FEATURES;INCLUDE_PRIVATE" ${ARGN})
    if(_target_args_SOURCES)
        target_sources(${_target} PRIVATE ${_target_args_SOURCES})
    endif()
    if(_target_args_LINKS)
        target_link_libraries(${_target} PUBLIC ${_target_args_LINKS})
    endif()
    if(_target_args_LINKS_PRIVATE)
        target_link_libraries(${_target} PRIVATE ${_target_args_LINKS_PRIVATE})
    endif()
    foreach(_module IN LISTS _target_args_QT_LINKS)
        find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS ${_module})
        target_link_libraries(${_target} PUBLIC Qt${QT_VERSION_MAJOR}::${_module})
    endforeach()
    foreach(_module IN LISTS _target_args_QT_INCLUDE_PRIVATE)
        if(QT_VERSION_MAJOR EQUAL 6)
            find_package(Qt6 REQUIRED COMPONENTS ${_module}Private)
        endif()
        target_link_libraries(${_target} PRIVATE Qt${QT_VERSION_MAJOR}::${_module}Private)
    endforeach()
    if(_target_args_FEATURES)
        target_compile_features(${_target} PUBLIC ${_target_args_FEATURES})
    endif()
    foreach(_directory IN LISTS _target_args_INCLUDE_PRIVATE)
        if(_directory STREQUAL "*")
            file(GLOB _entries LIST_DIRECTORIES true "${CMAKE_CURRENT_SOURCE_DIR}/*")
            foreach(_entry IN LISTS _entries)
                if(IS_DIRECTORY "${_entry}")
                    target_include_directories(${_target} PRIVATE "${_entry}")
                endif()
            endforeach()
        else()
            target_include_directories(${_target} PRIVATE "${_directory}")
        endif()
    endforeach()
endfunction()

function(qwk_add_library _target)
    cmake_parse_arguments(_library "QT_AUTOGEN" "MACRO_PREFIX" "" ${ARGN})
    if(QWINDOWKIT_BUILD_STATIC)
        add_library(${_target} STATIC)
        target_compile_definitions(${_target} PUBLIC ${_library_MACRO_PREFIX}_STATIC)
    else()
        add_library(${_target} SHARED)
    endif()
    target_compile_definitions(${_target} PRIVATE ${_library_MACRO_PREFIX}_LIBRARY)
    if(_library_QT_AUTOGEN)
        set_target_properties(${_target} PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON)
    endif()
    qwk_configure_target(${_target} ${_library_UNPARSED_ARGUMENTS})
    qm_add_win_rc(${_target} NAME ${QWINDOWKIT_INSTALL_NAME}
        DESCRIPTION "${QWINDOWKIT_DESCRIPTION}" COPYRIGHT "${QWINDOWKIT_COPYRIGHT}")
    target_include_directories(${_target} PRIVATE . ${QWINDOWKIT_BUILD_INCLUDE_DIR})
    if(WIN32 OR APPLE)
        target_compile_definitions(${_target} PRIVATE QWINDOWKIT_LINUX_H)
    endif()
    string(REGEX REPLACE "^QWK" "" _name "${_target}")
    set_target_properties(${_target} PROPERTIES EXPORT_NAME ${_name})
    add_library(${QWINDOWKIT_INSTALL_NAME}::${_name} ALIAS ${_target})
    if(QWINDOWKIT_INSTALL)
        install(TARGETS ${_target} EXPORT ${QWINDOWKIT_INSTALL_NAME}Targets
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
        target_include_directories(${_target} PUBLIC
            "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${QWINDOWKIT_INSTALL_NAME}>")
    endif()
endfunction()

function(qwk_sync_include _target)
    cmake_parse_arguments(_sync "FORCE" "" "EXCLUDE" ${ARGN})
    set(_destination ${QWINDOWKIT_GENERATED_INCLUDE_DIR}/${_target})
    file(GLOB_RECURSE _headers CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
    foreach(_header IN LISTS _headers)
        file(RELATIVE_PATH _relative "${QWindowKit_SOURCE_DIR}" "${_header}")
        set(_excluded false)
        foreach(_pattern IN LISTS _sync_EXCLUDE)
            if(_relative MATCHES "${_pattern}")
                set(_excluded true)
            endif()
        endforeach()
        get_filename_component(_name "${_header}" NAME)
        if(_name MATCHES "_p\\.h$")
            set(_output "${_destination}/private/${_name}")
        else()
            set(_output "${_destination}/${_name}")
        endif()
        if(_excluded)
            file(REMOVE "${_output}")
            continue()
        endif()
        configure_file("${_header}" "${_output}" COPYONLY)
    endforeach()
    target_include_directories(${_target} PUBLIC "$<BUILD_INTERFACE:${QWINDOWKIT_GENERATED_INCLUDE_DIR}>")
    if(QWINDOWKIT_INSTALL)
        install(DIRECTORY "${_destination}/" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${QWINDOWKIT_INSTALL_NAME}/${_target})
    endif()
endfunction()

function(qwk_install)
    if(NOT QWINDOWKIT_INSTALL)
        return()
    endif()
    set(_cmake_dir ${CMAKE_INSTALL_LIBDIR}/cmake/${QWINDOWKIT_INSTALL_NAME})
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/${QWINDOWKIT_INSTALL_NAME}Config.cmake.in" _config_template)
    if(QWINDOWKIT_BUILD_STATIC AND QT_VERSION_MAJOR EQUAL 6)
        string(REPLACE "include(CMakeFindDependencyMacro)"
            "include(CMakeFindDependencyMacro)\nfind_dependency(Qt6 REQUIRED COMPONENTS ${QWINDOWKIT_QT_PRIVATE_COMPONENTS})"
            _config_template "${_config_template}")
    endif()
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${QWINDOWKIT_INSTALL_NAME}Config.cmake.in" "${_config_template}")
    configure_package_config_file("${CMAKE_CURRENT_BINARY_DIR}/${QWINDOWKIT_INSTALL_NAME}Config.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/${QWINDOWKIT_INSTALL_NAME}Config.cmake"
        INSTALL_DESTINATION ${_cmake_dir})
    write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/${QWINDOWKIT_INSTALL_NAME}ConfigVersion.cmake"
        VERSION ${QWINDOWKIT_VERSION} COMPATIBILITY SameMajorVersion)
    install(EXPORT ${QWINDOWKIT_INSTALL_NAME}Targets
        NAMESPACE ${QWINDOWKIT_INSTALL_NAME}:: DESTINATION ${_cmake_dir})
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${QWINDOWKIT_INSTALL_NAME}Config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/${QWINDOWKIT_INSTALL_NAME}ConfigVersion.cmake" DESTINATION ${_cmake_dir})
endfunction()

macro(qwk_finish_buildsystem)
    string(APPEND _qwk_config_content "\n#endif // QWKCONFIG_H\n")
    file(WRITE "${QWINDOWKIT_BUILD_INCLUDE_DIR}/QWKCore/qwkconfig.h" "${_qwk_config_content}")
    configure_file("${QWINDOWKIT_BUILD_INCLUDE_DIR}/QWKCore/qwkconfig.h"
        "${QWINDOWKIT_GENERATED_INCLUDE_DIR}/QWKCore/qwkconfig.h" COPYONLY)
endmacro()
set(QT_VERSION_MAJOR ${QT_VERSION_MAJOR} PARENT_SCOPE)

# Native compatibility helpers for the existing example and test call sites.
function(qm_configure_target _target)
    qwk_configure_target(${_target} ${ARGN})
endfunction()

macro(qm_find_qt)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS ${ARGN})
endmacro()

function(qm_get_executable_location _target _output)
    get_target_property(_location ${_target} IMPORTED_LOCATION)
    if(NOT _location)
        get_target_property(_configurations ${_target} IMPORTED_CONFIGURATIONS)
        foreach(_configuration IN LISTS _configurations)
            get_target_property(_location ${_target} IMPORTED_LOCATION_${_configuration})
            if(_location)
                break()
            endif()
        endforeach()
    endif()
    set(${_output} "${_location}" PARENT_SCOPE)
endfunction()

function(qm_add_win_rc _target)
    get_target_property(_type ${_target} TYPE)
    if(NOT WIN32 OR NOT _type MATCHES "^(EXECUTABLE|SHARED_LIBRARY)$")
        return()
    endif()
    cmake_parse_arguments(_rc "" "NAME;DESCRIPTION;COPYRIGHT;ICON" "" ${ARGN})
    if(NOT _rc_NAME)
        set(_rc_NAME ${_target})
    endif()
    if(NOT _rc_DESCRIPTION)
        set(_rc_DESCRIPTION "${QWINDOWKIT_DESCRIPTION}")
    endif()
    string(REPLACE "." "," _version "${QWINDOWKIT_VERSION}")
    set(_resource "#include <windows.h>\n")
    if(_rc_ICON)
        string(APPEND _resource "IDI_ICON1 ICON DISCARDABLE \"${_rc_ICON}\"\n")
    endif()
    string(APPEND _resource "VS_VERSION_INFO VERSIONINFO\nFILEVERSION ${_version}\nPRODUCTVERSION ${_version}\n{
 BLOCK \"StringFileInfo\" {
  BLOCK \"040904E4\" {
   VALUE \"FileDescription\", \"${_rc_DESCRIPTION}\"
   VALUE \"FileVersion\", \"${QWINDOWKIT_VERSION}\"
   VALUE \"ProductName\", \"${_rc_NAME}\"
   VALUE \"ProductVersion\", \"${QWINDOWKIT_VERSION}\"
   VALUE \"LegalCopyright\", \"${_rc_COPYRIGHT}\"
  }
 }
 BLOCK \"VarFileInfo\" { VALUE \"Translation\", 0x409, 1252 }
}\n")
    set(_path "${CMAKE_CURRENT_BINARY_DIR}/${_target}_version.rc")
    file(WRITE "${_path}" "${_resource}")
    target_sources(${_target} PRIVATE "${_path}")
endfunction()

function(qm_add_win_manifest _target)
    if(NOT WIN32)
        return()
    endif()
    set(_manifest "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>
<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">
 <description>${QWINDOWKIT_DESCRIPTION}</description>
 <assemblyIdentity version=\"${QWINDOWKIT_VERSION}\" name=\"${_target}\" type=\"win32\"/>
 <trustInfo xmlns=\"urn:schemas-microsoft-com:asm.v3\"><security><requestedPrivileges>
  <requestedExecutionLevel level=\"asInvoker\" uiAccess=\"false\"/>
 </requestedPrivileges></security></trustInfo>
 <dependency><dependentAssembly><assemblyIdentity type=\"win32\" name=\"Microsoft.Windows.Common-Controls\" version=\"6.0.0.0\" processorArchitecture=\"*\" publicKeyToken=\"6595b64144ccf1df\" language=\"*\"/></dependentAssembly></dependency>
 <compatibility xmlns=\"urn:schemas-microsoft-com:compatibility.v1\"><application>\n")
    foreach(_build IN ITEMS 17763 18362 18363 19041 19042 19043 19044 19045 22000 22621 22631 26100 26200)
        string(APPEND _manifest "<maxversiontested Id=\"10.0.${_build}.0\"/>\n")
    endforeach()
    foreach(_os IN ITEMS e2011457-1546-43c5-a5fe-008deee3d3f0 35138b9a-5d96-4fbd-8e2d-a2440225f93a 4a2f28e3-53b9-4441-ba9c-d69d4a4a6e38 1f676c76-80e1-4239-95bb-83d0f6d0da78 8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a)
        string(APPEND _manifest "<supportedOS Id=\"{${_os}}\"/>\n")
    endforeach()
    string(APPEND _manifest "</application></compatibility>
 <application xmlns=\"urn:schemas-microsoft-com:asm.v3\"><windowsSettings>
  <dpiAware xmlns=\"http://schemas.microsoft.com/SMI/2005/WindowsSettings\">True/PM</dpiAware>
  <printerDriverIsolation xmlns=\"http://schemas.microsoft.com/SMI/2011/WindowsSettings\">True</printerDriverIsolation>
  <disableWindowFiltering xmlns=\"http://schemas.microsoft.com/SMI/2011/WindowsSettings\">True</disableWindowFiltering>
  <dpiAwareness xmlns=\"http://schemas.microsoft.com/SMI/2016/WindowsSettings\">PerMonitorV2, PerMonitor</dpiAwareness>
  <longPathAware xmlns=\"http://schemas.microsoft.com/SMI/2016/WindowsSettings\">True</longPathAware>
  <heapType xmlns=\"http://schemas.microsoft.com/SMI/2020/WindowsSettings\">SegmentHeap</heapType>
 </windowsSettings></application>
</assembly>\n")
    set(_path "${CMAKE_CURRENT_BINARY_DIR}/${_target}.manifest")
    file(WRITE "${_path}" "${_manifest}")
    if(MSVC)
        if(CMAKE_GENERATOR MATCHES "Visual Studio")
            target_link_options(${_target} PRIVATE "/manifest" "/manifestinput:${_path}" "/manifestuac:no")
        else()
            target_sources(${_target} PRIVATE "${_path}")
            target_link_options(${_target} PRIVATE "/manifestuac:no")
        endif()
    else()
        file(WRITE "${_path}.rc" "#include <windows.h>\n1 RT_MANIFEST \"${_path}\"\n")
        target_sources(${_target} PRIVATE "${_path}.rc")
    endif()
endfunction()

function(qm_add_mac_bundle _target)
    if(NOT APPLE)
        return()
    endif()
    cmake_parse_arguments(_bundle "" "ICON" "" ${ARGN})
    set_target_properties(${_target} PROPERTIES
        MACOSX_BUNDLE ON MACOSX_BUNDLE_BUNDLE_NAME ${_target}
        MACOSX_BUNDLE_GUI_IDENTIFIER ${_target}
        MACOSX_BUNDLE_BUNDLE_VERSION ${QWINDOWKIT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${QWINDOWKIT_VERSION}
        MACOSX_BUNDLE_INFO_STRING "${QWINDOWKIT_DESCRIPTION}"
        MACOSX_BUNDLE_COPYRIGHT "${QWINDOWKIT_COPYRIGHT}")
    if(_bundle_ICON)
        get_filename_component(_icon "${_bundle_ICON}" NAME)
        target_sources(${_target} PRIVATE "${_bundle_ICON}")
        set_source_files_properties("${_bundle_ICON}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
        set_target_properties(${_target} PROPERTIES MACOSX_BUNDLE_ICON_FILE "${_icon}")
    endif()
endfunction()

qwk_init_buildsystem(QWINDOWKIT)

set(QWINDOWKIT_ENABLED_TARGETS)
set(QWINDOWKIT_ENABLED_SUBDIRECTORIES)

# ----------------------------------
# Configurations
# ----------------------------------
qwk_add_definition(QWINDOWKIT_FORCE_QT_WINDOW_CONTEXT
    CONDITION QWINDOWKIT_FORCE_QT_WINDOW_CONTEXT
)
qwk_add_definition(QWINDOWKIT_ENABLE_STYLE_AGENT
    CONDITION QWINDOWKIT_ENABLE_STYLE_AGENT
)
qwk_add_definition(QWINDOWKIT_ENABLE_WINDOWS_SYSTEM_BORDERS
    CONDITION QWINDOWKIT_ENABLE_WINDOWS_SYSTEM_BORDERS
)

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
        set(_install_options INSTALL_DIR ${QWINDOWKIT_INSTALL_DOC_DIR})
    endif()

    set(_doc_targets ${QWINDOWKIT_ENABLED_TARGETS})

    set(QWINDOWKIT_DOXYGEN_TAGFILE
        ${QWINDOWKIT_BUILD_DOC_DIR}/${QWINDOWKIT_INSTALL_NAME}_tagfile.xml
        CACHE FILEPATH "QWindowKit doxygen tag file path" FORCE
    )

    set(DOXYGEN_PROJECT_NAME QWindowKit)
    set(DOXYGEN_PROJECT_BRIEF "${QWINDOWKIT_DESCRIPTION}")
    set(DOXYGEN_OUTPUT_DIRECTORY "${QWINDOWKIT_BUILD_DOC_DIR}")
    set(DOXYGEN_USE_MDFILE_AS_MAINPAGE "${CMAKE_CURRENT_SOURCE_DIR}/../README.md")
    set(DOXYGEN_GENERATE_TAGFILE "${QWINDOWKIT_DOXYGEN_TAGFILE}")
    set(DOXYGEN_PREDEFINED Q_SIGNALS=Q_SIGNALS Q_SLOTS=Q_SLOTS)
    doxygen_add_docs(QWindowKit_RunDoxygen ../README.md ${QWINDOWKIT_ENABLED_SUBDIRECTORIES})
    add_dependencies(QWindowKit_RunDoxygen ${_doc_targets})
    if(QWINDOWKIT_INSTALL)
        install(DIRECTORY "${QWINDOWKIT_BUILD_DOC_DIR}/" DESTINATION ${QWINDOWKIT_INSTALL_DOC_DIR} OPTIONAL)
    endif()
endif()

# ----------------------------------
# Install
# ----------------------------------
qwk_install()

qwk_finish_buildsystem()

if(QWINDOWKIT_INSTALL)
    # Install shared files
    include("../share/install.cmake")
endif()
