if(PLATFORM_WIN32)

    # Copies required dlls to the target's output directory
    #
    # The following dlls are copied:
    #  - Engine dlls (GraphicsEngine*.dll)
    #  - Archiver dll (Archiver*.dll)
    #  - D3Dcompiler_47.dll
    #  - Optional DXC dlls (dxcompiler.dll, dxil.dll, spv_dxcompiler.dll)
    #
    # Arguments:
    #   TARGET_NAME  - name of the target to copy dlls for
    #   DXC_REQUIRED - Indicates that the target requires DXC compiler dlls
    #                  (dxcompiler.dll, dxil.dll and spv_dxcompiler.dll)
    #
    # Example:
    #   copy_required_dlls(MyTarget DXC_REQUIRED YES)
    #
    function(copy_required_dlls TARGET_NAME)
        set(options)
        set(oneValueArgs DXC_REQUIRED)
        set(multiValueArgs)
        cmake_parse_arguments(PARSE_ARGV 1 arg "${options}" "${oneValueArgs}" "${multiValueArgs}")

        if(D3D11_SUPPORTED)
            list(APPEND ENGINE_DLLS Diligent-GraphicsEngineD3D11-shared)
        endif()
        if(D3D12_SUPPORTED)
            list(APPEND ENGINE_DLLS Diligent-GraphicsEngineD3D12-shared)
        endif()
        if(GL_SUPPORTED)
            list(APPEND ENGINE_DLLS Diligent-GraphicsEngineOpenGL-shared)
        endif()
        if(VULKAN_SUPPORTED)
            list(APPEND ENGINE_DLLS Diligent-GraphicsEngineVk-shared)
        endif()
        if(METAL_SUPPORTED)
            list(APPEND ENGINE_DLLS Diligent-GraphicsEngineMetal-shared)
        endif()
        if(WEBGPU_SUPPORTED)
            list(APPEND ENGINE_DLLS Diligent-GraphicsEngineWebGPU-shared)
        endif()
        if(TARGET Diligent-Archiver-shared)
            list(APPEND ENGINE_DLLS Diligent-Archiver-shared)
        endif()

        foreach(DLL ${ENGINE_DLLS})
            add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "\"$<TARGET_FILE:${DLL}>\""
                    "\"$<TARGET_FILE_DIR:${TARGET_NAME}>\"")
        endforeach(DLL)

        # Copy D3Dcompiler_47.dll, dxcompiler.dll, and dxil.dll
        if ((D3D11_SUPPORTED OR D3D12_SUPPORTED) AND D3D_COMPILER_PATH)
            list(APPEND SHADER_COMPILER_DLLS "${D3D_COMPILER_PATH}")
        endif()

        # Dawn uses DXC, so we need to copy the DXC dlls even if DXC_REQUIRED is not set
        # to get consistent shader compilation results
        if(((arg_DXC_REQUIRED AND D3D12_SUPPORTED) OR WEBGPU_SUPPORTED) AND DXC_COMPILER_PATH AND DXIL_SIGNER_PATH)
            # For the compiler to sign the bytecode, you have to have a copy of dxil.dll in
            # the same folder as the dxcompiler.dll at runtime.

            list(APPEND SHADER_COMPILER_DLLS "${DXC_COMPILER_PATH}")
            list(APPEND SHADER_COMPILER_DLLS "${DXIL_SIGNER_PATH}")
        endif()

        foreach(DLL ${SHADER_COMPILER_DLLS})
            add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${DLL}
                    "\"$<TARGET_FILE_DIR:${TARGET_NAME}>\"")
        endforeach(DLL)

        if(D3D12_SUPPORTED AND EXISTS "${DILIGENT_PIX_EVENT_RUNTIME_DLL_PATH}")
            add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${DILIGENT_PIX_EVENT_RUNTIME_DLL_PATH}"
                    "\"$<TARGET_FILE_DIR:${TARGET_NAME}>\"")
        endif()

        if(arg_DXC_REQUIRED AND VULKAN_SUPPORTED AND EXISTS "${DILIGENT_DXCOMPILER_FOR_SPIRV_PATH}")
            add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${DILIGENT_DXCOMPILER_FOR_SPIRV_PATH}"
                    "\"$<TARGET_FILE_DIR:${TARGET_NAME}>/spv_dxcompiler.dll\"")
        endif()
    endfunction()

elseif(PLATFORM_UNIVERSAL_WINDOWS)

    # Adds commands to package required DLLs into the appx package
    # Example:
    #   package_required_dlls(MyTarget DXC_REQUIRED YES)
    function(package_required_dlls TARGET_NAME)
        set(options)
        set(oneValueArgs DXC_REQUIRED)
        set(multiValueArgs)
        cmake_parse_arguments(PARSE_ARGV 1 arg "${options}" "${oneValueArgs}" "${multiValueArgs}")

        if(arg_DXC_REQUIRED AND D3D12_SUPPORTED AND DXC_COMPILER_PATH AND DXIL_SIGNER_PATH)
            # Copy the dlls to the project's CMake binary dir

            add_custom_command(TARGET ${TARGET_NAME} PRE_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${DXC_COMPILER_PATH}"
                    "${CMAKE_CURRENT_BINARY_DIR}/dxcompiler.dll"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${DXIL_SIGNER_PATH}"
                    "${CMAKE_CURRENT_BINARY_DIR}/dxil.dll")
            set(DLLS "${CMAKE_CURRENT_BINARY_DIR}/dxcompiler.dll" "${CMAKE_CURRENT_BINARY_DIR}/dxil.dll")

            # Add the dlls to the target project as source files
            target_sources(${TARGET_NAME} PRIVATE ${DLLS})

            # Label them as content
            set_source_files_properties(${DLLS} PROPERTIES
                GENERATED TRUE
                VS_DEPLOYMENT_CONTENT 1
                VS_DEPLOYMENT_LOCATION ".")
        endif()
    endfunction()

endif()

if (PLATFORM_WIN32 OR PLATFORM_UNIVERSAL_WINDOWS)

    # Set dll output name by adding _{32|64}{r|d} suffix
    function(set_dll_output_name TARGET_NAME OUTPUT_NAME_WITHOUT_SUFFIX)
        foreach(DBG_CONFIG ${DEBUG_CONFIGURATIONS})
            set_target_properties(${TARGET_NAME} PROPERTIES
                OUTPUT_NAME_${DBG_CONFIG} ${OUTPUT_NAME_WITHOUT_SUFFIX}${DLL_DBG_SUFFIX}
            )
        endforeach()

        foreach(REL_CONFIG ${RELEASE_CONFIGURATIONS})
            set_target_properties(${TARGET_NAME} PROPERTIES
                OUTPUT_NAME_${REL_CONFIG} ${OUTPUT_NAME_WITHOUT_SUFFIX}${DLL_REL_SUFFIX}
            )
        endforeach()
    endfunction()

endif()


function(set_common_target_properties TARGET)

    # Check if a second argument (MIN_CXX_STANDARD) is provided
    if(ARGC GREATER 1)
        set(MIN_CXX_STANDARD ${ARGV1})
    endif()

    if(COMMAND custom_pre_configure_target)
        custom_pre_configure_target(${TARGET})
        if(TARGET_CONFIGURATION_COMPLETE)
            return()
        endif()
    endif()

    get_target_property(TARGET_TYPE ${TARGET} TYPE)

    set(CXX_STANDARD 14)
    if(MIN_CXX_STANDARD)
        if(MIN_CXX_STANDARD GREATER ${CXX_STANDARD})
            set(CXX_STANDARD ${MIN_CXX_STANDARD})
        endif()
    endif()

    set_target_properties(${TARGET} PROPERTIES
        # It is crucial to set CXX_STANDARD flag to only affect c++ files and avoid failures compiling c-files:
        # error: invalid argument '-std=c++14' not allowed with 'C/ObjC'
        CXX_STANDARD ${CXX_STANDARD}
        CXX_STANDARD_REQUIRED ON
    )

    if(MSVC)
        # For msvc, enable link-time code generation for release builds (I was not able to
        # find any way to set these settings through interface library BuildSettings)
        if(TARGET_TYPE STREQUAL STATIC_LIBRARY)

            foreach(REL_CONFIG ${RELEASE_CONFIGURATIONS})
                set_target_properties(${TARGET} PROPERTIES
                    STATIC_LIBRARY_FLAGS_${REL_CONFIG} /LTCG
                )
            endforeach()

        else()

            foreach(REL_CONFIG ${RELEASE_CONFIGURATIONS})
                set_target_properties(${TARGET} PROPERTIES
                    LINK_FLAGS_${REL_CONFIG} "/LTCG /OPT:REF /INCREMENTAL:NO"
                )
            endforeach()

            if(PLATFORM_UNIVERSAL_WINDOWS)
                # On UWP, disable incremental link to avoid linker warnings
                foreach(DBG_CONFIG ${DEBUG_CONFIGURATIONS})
                    set_target_properties(${TARGET} PROPERTIES
                        LINK_FLAGS_${DBG_CONFIG} "/INCREMENTAL:NO"
                    )
                endforeach()
            endif()
        endif()
    else()
        set_target_properties(${TARGET} PROPERTIES
            CXX_VISIBILITY_PRESET hidden # -fvisibility=hidden
            C_VISIBILITY_PRESET hidden # -fvisibility=hidden
            VISIBILITY_INLINES_HIDDEN TRUE

            # Without -fPIC option GCC fails to link static libraries into dynamic library:
            #  -fPIC
            #      If supported for the target machine, emit position-independent code, suitable for
            #      dynamic linking and avoiding any limit on the size of the global offset table.
            POSITION_INDEPENDENT_CODE ON

            C_STANDARD 11 # MSVC requires legacy C standard
        )

        if(NOT MINGW_BUILD)
            # Do not disable extensions when building with MinGW!
            set_target_properties(${TARGET} PROPERTIES
                CXX_EXTENSIONS OFF
            )
        endif()

        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU" AND TARGET_TYPE STREQUAL SHARED_LIBRARY)
            target_link_options(${TARGET}
            PRIVATE
                # Disallow missing direct and indirect dependencies to ensure that .so is self-contained
                LINKER:--no-undefined
                LINKER:--no-allow-shlib-undefined
            )
        endif()
    endif() # if(MSVC)

    if (PLATFORM_WEB)
        if((${CMAKE_BUILD_TYPE} STREQUAL "Debug") AND (TARGET_TYPE STREQUAL EXECUTABLE) AND DILIGENT_EMSCRIPTEN_STRIP_DEBUG_INFO)
            # Strip debug info from WebAssembly binary. Without this option, the toolchain crashes on CI.
            target_link_options(${TARGET} PRIVATE "SHELL: -gseparate-dwarf -g0")
        endif()
    endif()

    if(COMMAND custom_post_configure_target)
        custom_post_configure_target(${TARGET})
    endif()

endfunction()

function(find_targets_in_directory _RESULT _DIR)
    get_property(_subdirs DIRECTORY "${_DIR}" PROPERTY SUBDIRECTORIES)
    foreach(_subdir IN LISTS _subdirs)
        find_targets_in_directory(${_RESULT} "${_subdir}")
    endforeach()
    get_property(_SUB_TARGETS DIRECTORY "${_DIR}" PROPERTY BUILDSYSTEM_TARGETS)
    set(${_RESULT} ${${_RESULT}} ${_SUB_TARGETS} PARENT_SCOPE)
endfunction()

function(set_directory_root_folder _DIRECTORY _ROOT_FOLDER)
    find_targets_in_directory(_TARGETS ${_DIRECTORY})
    foreach(_TARGET IN LISTS _TARGETS)
        get_target_property(_FOLDER ${_TARGET} FOLDER)
        if(_FOLDER)
            set_target_properties(${_TARGET} PROPERTIES FOLDER "${_ROOT_FOLDER}/${_FOLDER}")
        else()
            set_target_properties(${_TARGET} PROPERTIES FOLDER "${_ROOT_FOLDER}")
        endif()
    endforeach()
endfunction()


# Returns default backend library type (static/dynamic) for the current platform
function(get_backend_libraries_type _LIB_TYPE)
    if(PLATFORM_WIN32 OR PLATFORM_LINUX OR PLATFORM_ANDROID OR PLATFORM_UNIVERSAL_WINDOWS OR PLATFORM_MACOS)
        set(LIB_TYPE "shared")
    elseif(PLATFORM_IOS OR PLATFORM_TVOS OR PLATFORM_WEB)
        # Statically link with the engine on iOS, tvOS and Emscripten.
        # It is also possible to link dynamically by
        # putting the library into the framework.
        set(LIB_TYPE "static")
    else()
        message(FATAL_ERROR "Undefined platform")
    endif()
    set(${_LIB_TYPE} ${LIB_TYPE} PARENT_SCOPE)
endfunction()


# Adds the list of supported backend targets to variable ${_TARGETS} in parent scope.
# Second argument to the function may override the target type (static/dynamic). If It
# is not given, default target type for the platform is used.
function(get_supported_backends _TARGETS)
    if(${ARGC} GREATER 1)
        set(LIB_TYPE ${ARGV1})
    else()
        get_backend_libraries_type(LIB_TYPE)
    endif()

    if(D3D11_SUPPORTED)
        list(APPEND BACKENDS Diligent-GraphicsEngineD3D11-${LIB_TYPE})
    endif()
    if(D3D12_SUPPORTED)
        list(APPEND BACKENDS Diligent-GraphicsEngineD3D12-${LIB_TYPE})
    endif()
    if(GL_SUPPORTED OR GLES_SUPPORTED)
        list(APPEND BACKENDS Diligent-GraphicsEngineOpenGL-${LIB_TYPE})
    endif()
    if(VULKAN_SUPPORTED)
        list(APPEND BACKENDS Diligent-GraphicsEngineVk-${LIB_TYPE})
    endif()
    if(METAL_SUPPORTED)
        list(APPEND BACKENDS Diligent-GraphicsEngineMetal-${LIB_TYPE})
    endif()
    if(WEBGPU_SUPPORTED)
        list(APPEND BACKENDS Diligent-GraphicsEngineWebGPU-${LIB_TYPE})
    endif()

    # ${_TARGETS} == ENGINE_LIBRARIES
    # ${${_TARGETS}} == ${ENGINE_LIBRARIES}
    set(${_TARGETS} ${${_TARGETS}} ${BACKENDS} PARENT_SCOPE)
endfunction()


# Returns path to the target relative to CMake root
function(get_target_relative_dir _TARGET _DIR)
    get_target_property(TARGET_SOURCE_DIR ${_TARGET} SOURCE_DIR)
    file(RELATIVE_PATH TARGET_RELATIVE_PATH "${CMAKE_SOURCE_DIR}" "${TARGET_SOURCE_DIR}")
    set(${_DIR} ${TARGET_RELATIVE_PATH} PARENT_SCOPE)
endfunction()

# Performs installation steps for the core library
function(install_core_lib _TARGET)
    get_target_relative_dir(${_TARGET} TARGET_RELATIVE_PATH)

    get_target_property(TARGET_TYPE ${_TARGET} TYPE)
    if(TARGET_TYPE STREQUAL STATIC_LIBRARY)
        list(APPEND DILIGENT_CORE_INSTALL_LIBS_LIST ${_TARGET})
        set(DILIGENT_CORE_INSTALL_LIBS_LIST ${DILIGENT_CORE_INSTALL_LIBS_LIST} CACHE INTERNAL "Core libraries installation list")
    elseif(TARGET_TYPE STREQUAL SHARED_LIBRARY)
        install(TARGETS                 ${_TARGET}
                ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}/${DILIGENT_CORE_DIR}/$<CONFIG>"
                LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/${DILIGENT_CORE_DIR}/$<CONFIG>"
                RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}/${DILIGENT_CORE_DIR}/$<CONFIG>"
        )
        if (DILIGENT_INSTALL_PDB)
            install(FILES $<TARGET_PDB_FILE:${_TARGET}> DESTINATION "${CMAKE_INSTALL_BINDIR}/${DILIGENT_CORE_DIR}/$<CONFIG>" OPTIONAL)
        endif()
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/interface")
        install(DIRECTORY    interface
                DESTINATION  "${CMAKE_INSTALL_INCLUDEDIR}/${TARGET_RELATIVE_PATH}/"
        )
    endif()
endfunction()


function(install_combined_static_lib COMBINED_LIB_NAME LIBS_LIST CUSTOM_TARGET_NAME CUSTOM_TARGET_FOLDER INSTALL_DESTINATION)

    foreach(LIB ${LIBS_LIST})
        list(APPEND COMBINED_LIB_TARGET_FILES $<TARGET_FILE:${LIB}>)
    endforeach(LIB)

    if(MSVC)
        add_custom_command(
            OUTPUT "$<CONFIG>/${COMBINED_LIB_NAME}"
            COMMAND lib.exe /OUT:"$<CONFIG>/${COMBINED_LIB_NAME}" ${COMBINED_LIB_TARGET_FILES}
            DEPENDS ${LIBS_LIST}
            COMMENT "Combining libraries..."
        )
        add_custom_target(${CUSTOM_TARGET_NAME} ALL DEPENDS "$<CONFIG>/${COMBINED_LIB_NAME}")
        install(FILES "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/${COMBINED_LIB_NAME}"
                DESTINATION ${INSTALL_DESTINATION}
        )
    else()

        if(PLATFORM_WIN32)
            # do NOT use stock ar on MinGW
            find_program(AR NAMES x86_64-w64-mingw32-gcc-ar)
        else()
            set(AR ${CMAKE_AR})
        endif()

        if(AR)
            add_custom_command(
                OUTPUT ${COMBINED_LIB_NAME}
                # Delete all object files from current directory
                COMMAND ${CMAKE_COMMAND} -E remove "*${CMAKE_C_OUTPUT_EXTENSION}"
                DEPENDS ${LIBS_LIST}
                COMMENT "Combining libraries..."
            )

            # Unpack all object files from all targets to current directory
            foreach(LIB_TARGET ${COMBINED_LIB_TARGET_FILES})
                add_custom_command(
                    OUTPUT ${COMBINED_LIB_NAME}
                    COMMAND ${AR} -x ${LIB_TARGET}
                    APPEND
                )
            endforeach()

            # Pack object files to a combined library and delete them
            add_custom_command(
                OUTPUT ${COMBINED_LIB_NAME}
                COMMAND ${AR} -crs ${COMBINED_LIB_NAME} "*${CMAKE_C_OUTPUT_EXTENSION}"
                COMMAND ${CMAKE_COMMAND} -E remove "*${CMAKE_C_OUTPUT_EXTENSION}"
                APPEND
            )

            add_custom_target(${CUSTOM_TARGET_NAME} ALL DEPENDS ${COMBINED_LIB_NAME})
            install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${COMBINED_LIB_NAME}"
                    DESTINATION ${INSTALL_DESTINATION}
            )
        else()
            message("ar command is not found")
        endif()
    endif()

    if(TARGET ${CUSTOM_TARGET_NAME})
        set_target_properties(${CUSTOM_TARGET_NAME} PROPERTIES
            FOLDER ${CUSTOM_TARGET_FOLDER}
        )
    else()
        message("Unable to find librarian tool. Combined ${COMBINED_LIB_NAME} static library will not be produced.")
    endif()

endfunction()

function(add_format_validation_target MODULE_NAME MODULE_ROOT_PATH IDE_FOLDER)

    if(${DILIGENT_NO_FORMAT_VALIDATION})
        return()
    endif()

    add_custom_target(${MODULE_NAME}-ValidateFormatting ALL)

    if (NOT ("${DILIGENT_CORE_SOURCE_DIR}" STREQUAL "${MODULE_ROOT_PATH}"))
        # Start by copying .clang-format file to the module's root folder
        add_custom_command(TARGET ${MODULE_NAME}-ValidateFormatting
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DILIGENT_CORE_SOURCE_DIR}/.clang-format" "${MODULE_ROOT_PATH}/.clang-format"
        )
    endif()

    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
        set(RUN_VALIDATION_SCRIPT validate_format_win.bat)
    elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
        set(RUN_VALIDATION_SCRIPT ./validate_format_linux.sh)
    elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
        set(RUN_VALIDATION_SCRIPT ./validate_format_mac.sh)
    endif()

    if (RUN_VALIDATION_SCRIPT)
        # Run the format validation script
        add_custom_command(TARGET ${MODULE_NAME}-ValidateFormatting
            COMMAND ${RUN_VALIDATION_SCRIPT}
            WORKING_DIRECTORY "${MODULE_ROOT_PATH}/BuildTools/FormatValidation"
            COMMENT "Validating ${MODULE_NAME} module's source code formatting..."
            VERBATIM
        )

        if(TARGET ${MODULE_NAME}-ValidateFormatting)
            set_target_properties(${MODULE_NAME}-ValidateFormatting PROPERTIES FOLDER ${IDE_FOLDER})
        endif()
    else()
        message(DEBUG "${MODULE_NAME}-ValidateFormatting target will be disabled because format validation script is not available on ${CMAKE_HOST_SYSTEM_NAME} host platform.")
    endif()

endfunction()

# FetchContent's GIT_SHALLOW option is buggy and does not actually do a shallow
# clone. This macro takes care of it.
macro(FetchContent_DeclareShallowGit Name GIT_REPOSITORY GitRepository GIT_TAG GitTag)
    include(FetchContent)
    FetchContent_Declare(
        "${Name}"

        # This is what it'd look like if GIT_SHALLOW was indeed working:
        #GIT_REPOSITORY "${GitRepository}"
        #GIT_TAG        "${GitTag}"
        #GIT_SHALLOW    ON

        # Manual download mode instead:
        DOWNLOAD_COMMAND
            cd "${FETCHCONTENT_BASE_DIR}/${Name}-src" &&
            git init &&
            git fetch --depth=1 "${GitRepository}" "${GitTag}" &&
            git reset --hard FETCH_HEAD
    )
endmacro()

function(set_targets_emscripten_properties)
    if (PLATFORM_WEB)
        foreach(_TARGET IN LISTS ARGN)
            get_target_property(_TARGET_TYPE ${_TARGET} TYPE)
            if (_TARGET_TYPE STREQUAL "STATIC_LIBRARY")
                target_compile_options(${_TARGET} PRIVATE  
                    "-pthread"
                    "-mbulk-memory")
            endif()
        endforeach()
    endif()
endfunction()


# Links the static library <lib> to the target <tgt> as a whole archive.
function(target_link_whole_archive tgt lib)
    if(MSVC)
        target_link_libraries(${tgt} PRIVATE ${lib})
    else()
        if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
            # Use the new $<LINK_LIBRARY:WHOLE_ARCHIVE,lib> generator expression.
            # This one argument expands to the right flags automatically on
            # every toolchain (-Wl,--whole-archive/--no-whole-archive on ELF,
            # -Wl,-force_load on Apple, /WHOLEARCHIVE on MSVC, ...).
            target_link_libraries(
                ${tgt} PRIVATE
                "$<LINK_LIBRARY:WHOLE_ARCHIVE,${lib}>"
            )
        else()
            # Legacy toolchains: add the flags by hand.
            if(APPLE)
                target_link_libraries(
                    ${tgt} PRIVATE
                    "-Wl,-force_load,$<TARGET_FILE:${lib}>"
                )
            else()
                target_link_libraries(
                    ${tgt} PRIVATE
                    "-Wl,--whole-archive" ${lib} "-Wl,--no-whole-archive"
                )
            endif()
        endif()
    endif()
endfunction()
