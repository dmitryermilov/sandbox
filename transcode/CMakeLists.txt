set(PROJECT_NAME demo)

################################################################################
# Source groups
################################################################################
set(Headers
	"include/transcode.h"
	"include/device.h"
	"include/base_allocator.h"
	"include/d3d11_allocator.h"
    "include/common_utils.h"
	"include/cmd_options.h"
)
source_group("Headers" FILES ${Headers})

set(Sources
    "src/demo.cpp"
	"src/base_allocator.cpp"
	"src/d3d11_allocator.cpp"
	"src/common_utils.cpp"
	"src/cmd_options.cpp"
)
source_group("Sources" FILES ${Sources})

set(ALL_FILES
    ${Headers}
    ${Sources}
)

################################################################################
# Target
################################################################################
add_executable(${PROJECT_NAME} ${ALL_FILES})

use_props(${PROJECT_NAME} "${CMAKE_CONFIGURATION_TYPES}" "${DEFAULT_CXX_PROPS}")
################################################################################

set(ROOT_NAMESPACE transcode)

set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_GLOBAL_KEYWORD "Win32Proj"
)
################################################################################
# Target name
################################################################################
if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "Win32")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        TARGET_NAME_DEBUG_WITHDEBUGAPI "transcode"
        TARGET_NAME_DEBUG              "transcode"
        TARGET_NAME_RELEASE            "transcode"
    )
elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        TARGET_NAME_DEBUG_WITHDEBUGAPI "transcode"
        TARGET_NAME_DEBUG              "transcode"
        TARGET_NAME_RELEASE            "transcode"
    )
endif()
################################################################################
# Output directory
################################################################################
if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "Win32")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        OUTPUT_DIRECTORY_DEBUG_WITHDEBUGAPI "${CMAKE_CURRENT_SOURCE_DIR}/../build/win_${CMAKE_VS_PLATFORM_NAME}/$<CONFIG>/bin/"
        OUTPUT_DIRECTORY_DEBUG              "${CMAKE_CURRENT_SOURCE_DIR}/../build/win_${CMAKE_VS_PLATFORM_NAME}/$<CONFIG>/bin/"
        OUTPUT_DIRECTORY_RELEASE            "${CMAKE_CURRENT_SOURCE_DIR}/../build/win_${CMAKE_VS_PLATFORM_NAME}/$<CONFIG>/bin/"
    )
elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        OUTPUT_DIRECTORY_DEBUG_WITHDEBUGAPI "${CMAKE_CURRENT_SOURCE_DIR}/../build/win_${CMAKE_VS_PLATFORM_NAME}/$<CONFIG>/bin/"
        OUTPUT_DIRECTORY_DEBUG              "${CMAKE_CURRENT_SOURCE_DIR}/../build/win_${CMAKE_VS_PLATFORM_NAME}/$<CONFIG>/bin/"
        OUTPUT_DIRECTORY_RELEASE            "${CMAKE_CURRENT_SOURCE_DIR}/../build/win_${CMAKE_VS_PLATFORM_NAME}/$<CONFIG>/bin/"
    )
endif()
if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        PDB_OUTPUT_DIRECTORY_DEBUG_WITHDEBUGAPI "${OUTPUT_DIRECTORY}"
        PDB_OUTPUT_DIRECTORY_DEBUG              "${OUTPUT_DIRECTORY}"
    )
endif()
if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "Win32")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        INTERPROCEDURAL_OPTIMIZATION_RELEASE            "TRUE"
    )
elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        INTERPROCEDURAL_OPTIMIZATION_RELEASE            "TRUE"
    )
endif()
################################################################################
# MSVC runtime library
################################################################################
get_property(MSVC_RUNTIME_LIBRARY_DEFAULT TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY)
if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "Win32")
    string(CONCAT "MSVC_RUNTIME_LIBRARY_STR"
        $<$<CONFIG:Debug_WithDebugAPI>:
            MultiThreadedDebug
        >
        $<$<CONFIG:Debug>:
            MultiThreadedDebug
        >
        $<$<CONFIG:Release>:
            MultiThreaded
        >
        $<$<NOT:$<OR:$<CONFIG:Debug_WithDebugAPI>,$<CONFIG:Debug>,$<CONFIG:Release>>>:${MSVC_RUNTIME_LIBRARY_DEFAULT}>
    )
elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
    string(CONCAT "MSVC_RUNTIME_LIBRARY_STR"
        $<$<CONFIG:Debug_WithDebugAPI>:
            MultiThreadedDebug
        >
        $<$<CONFIG:Debug>:
            MultiThreadedDebug
        >
        $<$<CONFIG:Release>:
            MultiThreaded
        >
        $<$<NOT:$<OR:$<CONFIG:Debug_WithDebugAPI>,$<CONFIG:Debug>,$<CONFIG:Release>>>:${MSVC_RUNTIME_LIBRARY_DEFAULT}>
    )
endif()
set_target_properties(${PROJECT_NAME} PROPERTIES MSVC_RUNTIME_LIBRARY ${MSVC_RUNTIME_LIBRARY_STR})

################################################################################
# Include directories
################################################################################
if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "Win32")
    target_include_directories(${PROJECT_NAME} PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include;"
        "$ENV{INTELMEDIASDKROOT}/include;"
        "${CMAKE_CURRENT_SOURCE_DIR}/../sample_common/include;"
    )
elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
    target_include_directories(${PROJECT_NAME} PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include;"
        "$ENV{INTELMEDIASDKROOT}/include;"
        "${CMAKE_CURRENT_SOURCE_DIR}/../sample_common/include;"
    )
endif()

################################################################################
# Compile definitions
################################################################################
if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "Win32")
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        "$<$<CONFIG:Debug_WithDebugAPI>:"
            "_DEBUG"
        ">"
        "$<$<CONFIG:Debug>:"
            "_DEBUG"
        ">"
        "$<$<CONFIG:Release>:"
            "NDEBUG;"
            "SAVE_RECON;"
            "MFX_D3D11_SUPPORT=1"
        ">"
        "WIN32;"
        "_CONSOLE;"
        "NOMINMAX;"
        "UNICODE;"
        "_UNICODE"
    )
elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        "$<$<CONFIG:Debug_WithDebugAPI>:"
            "WIN32;"
            "_DEBUG"
        ">"
        "$<$<CONFIG:Debug>:"
            "WIN32;"
            "_DEBUG"
        ">"
        "$<$<CONFIG:Release>:"
            "NDEBUG;"
            "SAVE_RECON;"
            "MFX_D3D11_SUPPORT=1"
        ">"
        "_CONSOLE;"
        "NOMINMAX;"
        "UNICODE;"
        "_UNICODE"
    )
endif()

################################################################################
# Compile and link options
################################################################################
if(MSVC)
    if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "Win32")
        target_compile_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Debug_WithDebugAPI>:
                /Gm;
                /Od;
                /RTC1;
                /Zi
            >
            $<$<CONFIG:Debug>:
                /Gm;
                /Od;
                /RTC1;
                /Zi
            >
            $<$<CONFIG:Release>:
                /GL-;
                ${DEFAULT_CXX_DEBUG_INFORMATION_FORMAT}
            >
            /W4;
            /WX-;
            ${DEFAULT_CXX_EXCEPTION_HANDLING};
            /Y-
        )
    elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
        target_compile_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Debug_WithDebugAPI>:
                /Od;
                /Zi
            >
            $<$<CONFIG:Debug>:
                /Od;
                /Zi
            >
            $<$<CONFIG:Release>:
                ${DEFAULT_CXX_DEBUG_INFORMATION_FORMAT}
            >
            /W4;
            /WX-;
            ${DEFAULT_CXX_EXCEPTION_HANDLING}
        )
    endif()
    if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "Win32")
        target_link_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Debug_WithDebugAPI>:
                /NODEFAULTLIB:LIBCMT.lib;
                /DEBUG;
                /INCREMENTAL
            >
            $<$<CONFIG:Debug>:
                /NODEFAULTLIB:LIBCMT.lib;
                /DEBUG;
                /INCREMENTAL
            >
            $<$<CONFIG:Release>:
                /OPT:REF;
                /OPT:ICF;
                /INCREMENTAL:NO
            >
            /NODEFAULTLIB:msvcrt.lib;
            /NODEFAULTLIB:msvcrtd.lib;
            /MACHINE:X86;
            /SUBSYSTEM:CONSOLE;
            /DYNAMICBASE;
            /NXCOMPAT;
            /SAFESEH
        )
    elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
        target_link_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Debug_WithDebugAPI>:
                /NODEFAULTLIB:LIBCMT.lib;
                /DEBUG;
                /INCREMENTAL
            >
            $<$<CONFIG:Debug>:
                /NODEFAULTLIB:LIBCMT.lib;
                /DEBUG;
                /INCREMENTAL
            >
            $<$<CONFIG:Release>:
                /INCREMENTAL:NO
            >
            /NODEFAULTLIB:msvcrt.lib;
            /NODEFAULTLIB:msvcrtd.lib;
            /SUBSYSTEM:CONSOLE;
            /DYNAMICBASE;
            /NXCOMPAT
        )
    endif()
endif()

################################################################################
# Dependencies
################################################################################


if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "Win32")
    set(ADDITIONAL_LIBRARY_DEPENDENCIES
        "libmfx_vs2015;"
        "dxva2;"
        "d3d9;"
        "d3d11;"
        "dxgi;"
        "psapi"
    )
elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
    set(ADDITIONAL_LIBRARY_DEPENDENCIES
        "libmfx_vs2015;"
        "dxva2;"
        "d3d9;"
        "d3d11;"
        "dxgi;"
        "psapi"
    )
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE "${ADDITIONAL_LIBRARY_DEPENDENCIES}")

if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "Win32")
    target_link_directories(${PROJECT_NAME} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../build/win_${CMAKE_VS_PLATFORM_NAME}/$<CONFIG>/lib/;"
        "$ENV{INTELMEDIASDKROOT}/lib/${CMAKE_VS_PLATFORM_NAME}"
    )
elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
    target_link_directories(${PROJECT_NAME} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../build/win_${CMAKE_VS_PLATFORM_NAME}/$<CONFIG>/lib/;"
        "$ENV{INTELMEDIASDKROOT}/lib/${CMAKE_VS_PLATFORM_NAME}"
    )
endif()

