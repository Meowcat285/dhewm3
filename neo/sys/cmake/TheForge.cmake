# Builds the parts of The Forge (https://github.com/ConfettiFX/The-Forge) that
# dhewm3 uses as a static library called "TheForge".
#
# The Forge ships Visual Studio / CodeLite / Xcode projects but no CMake, so the
# source lists below mirror these projects from the The Forge tree:
#   Examples_3/Unit_Tests/PC_VS2019/Libraries/OS/OS.vcxproj
#   Examples_3/Unit_Tests/PC_VS2019/Libraries/Tests/RendererVulkan.vcxproj
#   Examples_3/Unit_Tests/SteamOS_CodeLite/OSBase/OSBase.project
#   Examples_3/Unit_Tests/SteamOS_CodeLite/Renderer/Renderer.project
# trimmed down to what a renderer embedded in another engine needs: no Forge
# app framework (dhewm3 owns the window via SDL), no Lua, UI, fonts, profiler,
# animation or input.
#
# The Forge's build configuration is replaced by renderer/forge/ForgeConfig.h
# through EXTERNAL_CONFIG_FILEPATH, so the The Forge sources stay unmodified.
#
# Input:  THEFORGE_DIR  - root of The Forge checkout (neo/libs/TheForge by default)
#         FORGE_DEBUG   - build The Forge in debug mode (Vulkan validation etc.)
# Output: TheForge      - static library target; linking it also provides the
#                         include dirs and compile definitions needed to use
#                         The Forge headers.

set(TF_COMMON "${THEFORGE_DIR}/Common_3")

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
	message(FATAL_ERROR "FORGE_RENDERER needs a 64-bit build. Disable FORGE_RENDERER for 32-bit builds (OpenGL renderer).")
endif()
if(TOOLS)
	message(FATAL_ERROR "The Windows editor tools (TOOLS) use OpenGL directly and don't work with FORGE_RENDERER (yet). Disable one of them.")
endif()

if(NOT EXISTS "${TF_COMMON}/Graphics/Interfaces/IGraphics.h")
	message(FATAL_ERROR "The Forge not found in ${THEFORGE_DIR}.\n"
		"Run `git submodule update --init` or set THEFORGE_DIR to a The Forge checkout.")
endif()

# --- Utilities (memory, logging, file system, threads, math helpers) ---
set(tf_src_utilities
	${TF_COMMON}/Utilities/FileSystem/FileSystem.c
	${TF_COMMON}/Utilities/Log/Log.c
	${TF_COMMON}/Utilities/Math/Algorithms.c
	${TF_COMMON}/Utilities/Math/StbDs.c
	${TF_COMMON}/Utilities/MemoryTracking/MemoryTracking.c
	${TF_COMMON}/Utilities/Threading/ThreadSystem.c
	${TF_COMMON}/Utilities/Timer.c
	${TF_COMMON}/Utilities/ThirdParty/OpenSource/bstrlib/bstrlib.c
	${TF_COMMON}/Utilities/ThirdParty/OpenSource/lz4/lz4.c
	${TF_COMMON}/Utilities/ThirdParty/OpenSource/zstd/common/debug.c
	${TF_COMMON}/Utilities/ThirdParty/OpenSource/zstd/common/entropy_common.c
	${TF_COMMON}/Utilities/ThirdParty/OpenSource/zstd/common/error_private.c
	${TF_COMMON}/Utilities/ThirdParty/OpenSource/zstd/common/fse_decompress.c
	${TF_COMMON}/Utilities/ThirdParty/OpenSource/zstd/common/pool.c
	${TF_COMMON}/Utilities/ThirdParty/OpenSource/zstd/common/threading.c
	${TF_COMMON}/Utilities/ThirdParty/OpenSource/zstd/common/xxhash.c
	${TF_COMMON}/Utilities/ThirdParty/OpenSource/zstd/common/zstd_common.c
	${TF_COMMON}/Utilities/ThirdParty/OpenSource/zstd/decompress/huf_decompress.c
	${TF_COMMON}/Utilities/ThirdParty/OpenSource/zstd/decompress/zstd_ddict.c
	${TF_COMMON}/Utilities/ThirdParty/OpenSource/zstd/decompress/zstd_decompress.c
	${TF_COMMON}/Utilities/ThirdParty/OpenSource/zstd/decompress/zstd_decompress_block.c
)

# --- OS layer (platform file system, logging, threads, time, CPU info) ---
set(tf_src_os
	${TF_COMMON}/OS/CPUConfig.cpp
)
if(WIN32)
	set(tf_src_os ${tf_src_os}
		${TF_COMMON}/OS/Windows/WindowsFileSystem.cpp
		${TF_COMMON}/OS/Windows/WindowsLog.c
		${TF_COMMON}/OS/Windows/WindowsThread.c
		${TF_COMMON}/OS/Windows/WindowsTime.c
		${TF_COMMON}/OS/ThirdParty/OpenSource/cpu_features/src/impl_x86_windows.c
	)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	set(tf_src_os ${tf_src_os}
		${TF_COMMON}/Utilities/FileSystem/UnixFileSystem.c
		${TF_COMMON}/OS/Linux/LinuxFileSystem.c
		${TF_COMMON}/OS/Linux/LinuxLog.c
		${TF_COMMON}/OS/Linux/LinuxThread.c
		${TF_COMMON}/OS/Linux/LinuxTime.c
		${TF_COMMON}/OS/ThirdParty/OpenSource/cpu_features/src/impl_x86_linux_or_android.c
		${TF_COMMON}/OS/ThirdParty/OpenSource/cpu_features/src/impl_aarch64_linux_or_android.c
	)
else()
	message(FATAL_ERROR "FORGE_RENDERER: only Windows and Linux are supported for now.")
endif()

# --- Graphics: Vulkan backend + GPU config + resource loader ---
set(tf_src_graphics
	${TF_COMMON}/Graphics/GraphicsConfig.cpp
	${TF_COMMON}/Graphics/Vulkan/Vulkan.c
	${TF_COMMON}/Graphics/Vulkan/Vulkan_cxx.cpp
	${TF_COMMON}/Graphics/Vulkan/VulkanRaytracing.c
	${TF_COMMON}/Resources/ResourceLoader/ResourceLoader.cpp
)

add_library(TheForge STATIC ${tf_src_utilities} ${tf_src_os} ${tf_src_graphics})
source_group(TREE ${TF_COMMON} PREFIX TheForge FILES ${tf_src_utilities} ${tf_src_os} ${tf_src_graphics})

set_target_properties(TheForge PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED ON
	POSITION_INDEPENDENT_CODE ON
)
if(NOT MSVC)
	# The Forge's own MSVC projects use the compiler's default C mode
	# (/std:c11 would also switch on the conforming preprocessor)
	set_target_properties(TheForge PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
endif()

# Definitions needed by everything that includes The Forge headers
set(tf_public_defs
	EXTERNAL_CONFIG_FILEPATH="${CMAKE_SOURCE_DIR}/renderer/forge/ForgeConfig.h"
)
if(WIN32)
	# On Windows The Forge defaults to D3D12; select Vulkan explicitly.
	set(tf_public_defs ${tf_public_defs}
		FORGE_EXPLICIT_RENDERER_API
		FORGE_EXPLICIT_RENDERER_API_VULKAN
	)
endif()
if(FORGE_DEBUG)
	set(tf_public_defs ${tf_public_defs} DHEWM3_FORGE_DEBUG)
endif()

target_compile_definitions(TheForge PUBLIC ${tf_public_defs})
# zstd's x86_64 assembly (.S) isn't part of the source list
target_compile_definitions(TheForge PRIVATE ZSTD_DISABLE_ASM)
target_include_directories(TheForge PUBLIC "${TF_COMMON}")

if(MSVC)
	target_compile_definitions(TheForge PRIVATE
		_CRT_SECURE_NO_WARNINGS
		_CRT_NONSTDC_NO_DEPRECATE
		_HAS_EXCEPTIONS=0
	)
	# same as The Forge's TF_Shared.props, minus warnings-as-errors
	target_compile_options(TheForge PRIVATE /W3 /wd4201 /wd4324 /wd4127 /GR- /MP)
else()
	# The Forge code is not ours, keep its warnings out of our build log
	target_compile_options(TheForge PRIVATE -w -fno-exceptions
		$<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>)
endif()

find_package(Threads REQUIRED)
target_link_libraries(TheForge PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
if(WIN32)
	# Vulkan itself is loaded at runtime through volk, so no vulkan-1.lib is needed.
	# The Vulkan backend uses AMD AGS and NVIDIA NVAPI on Windows (driver info, workarounds).
	# nvapi64.lib is static, AGS needs amd_ags_x64.dll next to the executable (see THEFORGE_RUNTIME_FILES).
	set(TF_AGS_DIR "${TF_COMMON}/Graphics/ThirdParty/OpenSource/ags/ags_lib/lib")
	set(TF_NVAPI_DIR "${TF_COMMON}/Graphics/ThirdParty/OpenSource/nvapi/amd64")
	target_link_libraries(TheForge PUBLIC ws2_32 shlwapi
		"${TF_AGS_DIR}/amd_ags_x64.lib"
		"${TF_NVAPI_DIR}/nvapi64.lib"
	)
	set(THEFORGE_RUNTIME_FILES "${TF_AGS_DIR}/amd_ags_x64.dll")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	find_package(X11 REQUIRED)
	target_include_directories(TheForge PUBLIC ${X11_INCLUDE_DIR})
	target_link_libraries(TheForge PUBLIC ${X11_LIBRARIES})
endif()
