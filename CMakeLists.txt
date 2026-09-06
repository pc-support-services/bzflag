# CMake build for BZFlag - ARM64 Windows native build
# This CMakeLists.txt enables building BZFlag as a native ARM64 binary
# on Windows on ARM, eliminating x64 emulation overhead.
cmake_minimum_required(VERSION 3.20)
project(BZFlag VERSION 2.4.31 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use static GLEW
add_compile_definitions(GLEW_STATIC)

if(WIN32)
    add_compile_definitions(_WINDOWS WIN32 _WINSOCK_DEPRECATED_NO_WARNINGS)
    add_compile_definitions(_CRT_SECURE_NO_DEPRECATE _CRT_NONSTDC_NO_DEPRECATE)
    add_compile_definitions(HAVE_REGEX_H HAVE_STRICMP HAVE_STD__COUNT HAVE_STD__ISNAN)
    add_compile_definitions(HAVE_ASINF HAVE_ATAN2F HAVE_ATANF HAVE_COSF HAVE_EXPF)
    add_compile_definitions(HAVE_FABSF HAVE_FLOORF HAVE_FMODF HAVE_LOGF HAVE_POWF)
    add_compile_definitions(HAVE_SINF HAVE_SQRTF HAVE_TANF HAVE_LOG10F)
    add_compile_definitions(HAVE_WAITFORSINGLEOBJECT HAVE_SLEEP)
    add_compile_definitions(HAVE__STRICMP HAVE__STRNICMP HAVE__VSNPRINTF)
    add_compile_definitions(BZ_PLUGINS HAVE_SDL2_SDL_H HAVE_STD__MIN HAVE_STD__MAX)
    add_compile_definitions(HAVE_ARES_GETADDRINFO ROBOT)
    add_compile_definitions(INSTALL_DATA_DIR="${CMAKE_SOURCE_DIR}/data")
    
    # ARM64 build OS string
    if(CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64" OR CMAKE_VS_PLATFORM_NAME STREQUAL "ARM64")
        add_compile_definitions(BZ_BUILD_OS="WinARM64VC14")
        message(STATUS "Building native ARM64 binary - no x64 emulation overhead")
    elseif(CMAKE_GENERATOR_PLATFORM STREQUAL "x64" OR CMAKE_VS_PLATFORM_NAME STREQUAL "x64")
        add_compile_definitions(BZ_BUILD_OS="Win64VC14")
    else()
        add_compile_definitions(BZ_BUILD_OS="Win32VC14")
    endif()
endif()

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/MSVC/build
)

# Find dependencies via vcpkg or system
find_package(ZLIB REQUIRED)
find_package(c-ares CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(SDL2 CONFIG REQUIRED)

# GLEW - built manually for ARM64, no cmake config available
# Just use the headers and link the static lib directly
find_path(GLEW_INCLUDE_DIR GL/glew.h
    HINTS ${CMAKE_SOURCE_DIR}/../vcpkg/installed/arm64-windows/include
)
find_library(GLEW_LIBRARY NAMES glew32s libglew32
    HINTS ${CMAKE_SOURCE_DIR}/../vcpkg/installed/arm64-windows/lib
)
if(GLEW_INCLUDE_DIR AND GLEW_LIBRARY)
    message(STATUS "Found GLEW: ${GLEW_LIBRARY}")
    add_library(GLEW::GLEW STATIC IMPORTED)
    set_target_properties(GLEW::GLEW PROPERTIES
        IMPORTED_LOCATION "${GLEW_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${GLEW_INCLUDE_DIR}"
    )
else()
    find_package(GLEW REQUIRED)
endif()

# OpenGL
find_package(OpenGL REQUIRED)

# Common source files - libraries
set(COMMON_SRCS
    src/common/AccessList.cxx
    src/common/AutoCompleter.cxx
    src/common/Bundle.cxx
    src/common/BundleMgr.cxx
    src/common/bz_Locale.cxx
    src/common/BZDBCache.cxx
    src/common/BZDBLocal.cxx
    src/common/bzfio.cxx
    src/common/bzglob.cxx
    src/common/BzPNG.cxx
    src/common/bzsignal.cxx
    src/common/CommandManager.cxx
    src/common/ConfigFileManager.cxx
    src/common/Country.cxx
    src/common/cURLManager.cxx
    src/common/ErrorHandler.cxx
    src/common/FileManager.cxx
    src/common/Flag.cxx
    src/common/GameTime.cxx
    src/common/global.cxx
    src/common/KeyManager.cxx
    src/common/Language.cxx
    src/common/mathRoutine.cxx
    src/common/md5.cxx
    src/common/messages.cxx
    src/common/OSFile.cxx
    src/common/ParseColor.cxx
    src/common/PlayerState.cxx
    src/common/ShotUpdate.cxx
    src/common/StateDatabase.cxx
    src/common/Team.cxx
    src/common/TextChunkManager.cxx
    src/common/TextUtils.cxx
    src/common/TimeKeeper.cxx
    src/common/VotingBooth.cxx
    src/common/WordFilter.cxx
)

set(GAME_SRCS
    src/game/BzMaterial.cxx
    src/game/CacheManager.cxx
    src/game/CollisionManager.cxx
    src/game/CommandsStandard.cxx
    src/game/DirectoryNames.cxx
    src/game/DynamicColor.cxx
    src/game/Frustum.cxx
    src/game/Intersect.cxx
    src/game/LagInfo.cxx
    src/game/LinkManager.cxx
    src/game/MeshTransform.cxx
    src/game/MsgStrings.cxx
    src/game/NetHandler.cxx
    src/game/PhysicsDriver.cxx
    src/game/PlayerInfo.cxx
    src/game/Ray.cxx
    src/game/ServerAuth.cxx
    src/game/ServerItem.cxx
    src/game/ServerList.cxx
    src/game/ServerListCache.cxx
    src/game/StartupInfo.cxx
    src/game/TextureMatrix.cxx
)

set(NET_SRCS
    src/net/Address.cxx
    src/net/AresHandler.cxx
    src/net/multicast.cxx
    src/net/network.cxx
    src/net/Pack.cxx
    src/net/Ping.cxx
)

set(OBSTACLE_SRCS
    src/obstacle/ArcObstacle.cxx
    src/obstacle/BaseBuilding.cxx
    src/obstacle/BaseSceneNodeGenerator.cxx
    src/obstacle/BoxBuilding.cxx
    src/obstacle/BoxSceneNodeGenerator.cxx
    src/obstacle/ConeObstacle.cxx
    src/obstacle/EmptySceneNodeGenerator.cxx
    src/obstacle/MeshDrawInfo.cxx
    src/obstacle/MeshFace.cxx
    src/obstacle/MeshObstacle.cxx
    src/obstacle/MeshSceneNodeGenerator.cxx
    src/obstacle/Obstacle.cxx
    src/obstacle/ObstacleList.cxx
    src/obstacle/ObstacleMgr.cxx
    src/obstacle/ObstacleModifier.cxx
    src/obstacle/ObstacleSceneNodeGenerator.cxx
    src/obstacle/PyramidBuilding.cxx
    src/obstacle/PyramidSceneNodeGenerator.cxx
    src/obstacle/SphereObstacle.cxx
    src/obstacle/Teleporter.cxx
    src/obstacle/TeleporterSceneNodeGenerator.cxx
    src/obstacle/TetraBuilding.cxx
    src/obstacle/Triangulate.cxx
    src/obstacle/WallObstacle.cxx
    src/obstacle/WallSceneNodeGenerator.cxx
)

set(PLATFORM_SRCS
    src/platform/BzfDisplay.cxx
    src/platform/BzfJoystick.cxx
    src/platform/BzfMedia.cxx
    src/platform/BzfVisual.cxx
    src/platform/BzfWindow.cxx
    src/platform/PlatformFactory.cxx
    src/platform/SDL2Display.cxx
    src/platform/SDL2Visual.cxx
    src/platform/SDL2Window.cxx
    src/platform/SDLJoystick.cxx
    src/platform/SDLMedia.cxx
    src/platform/SDLPlatformFactory.cxx
)

if(WIN32)
    set(PLATFORM_SRCS ${PLATFORM_SRCS}
        src/platform/NewMedia.cxx
    )
endif()

set(OGL_SRCS
    src/ogl/OpenGLFramebuffer.cxx
    src/ogl/OpenGLGState.cxx
    src/ogl/OpenGLLight.cxx
    src/ogl/OpenGLMaterial.cxx
    src/ogl/OpenGLTexture.cxx
    src/ogl/OpenGLUtils.cxx
    src/ogl/RenderNode.cxx
)

set(SCENE_SRCS
    src/scene/BSPSceneDatabase.cxx
    src/scene/Occluder.cxx
    src/scene/Octree.cxx
    src/scene/SceneDatabase.cxx
    src/scene/ZSceneDatabase.cxx
)

set(GEOMETRY_SRCS
    src/geometry/AnimatedTreads.cxx
    src/geometry/BillboardSceneNode.cxx
    src/geometry/BoltSceneNode.cxx
    src/geometry/EighthDBaseSceneNode.cxx
    src/geometry/EighthDBoxSceneNode.cxx
    src/geometry/EighthDimSceneNode.cxx
    src/geometry/EighthDimShellNode.cxx
    src/geometry/EighthDPyrSceneNode.cxx
    src/geometry/FlagSceneNode.cxx
    src/geometry/FlagWarpSceneNode.cxx
    src/geometry/LaserSceneNode.cxx
    src/geometry/MeshDrawMgr.cxx
    src/geometry/MeshFragSceneNode.cxx
    src/geometry/MeshPolySceneNode.cxx
    src/geometry/MeshRenderNode.cxx
    src/geometry/MeshSceneNode.cxx
    src/geometry/OccluderSceneNode.cxx
    src/geometry/PolyWallSceneNode.cxx
    src/geometry/QuadWallSceneNode.cxx
    src/geometry/SceneNode.cxx
    src/geometry/SphereSceneNode.cxx
    src/geometry/TankGeometryMgr.cxx
    src/geometry/TankSceneNode.cxx
    src/geometry/TriWallSceneNode.cxx
    src/geometry/ViewFrustum.cxx
    src/geometry/WallSceneNode.cxx
)

set(THREED_SRCS
    src/3D/FontManager.cxx
    src/3D/ImageFont.cxx
    src/3D/TextureFont.cxx
    src/3D/TextureManager.cxx
)

set(MEDIAFILE_SRCS
    src/mediafile/AudioFile.cxx
    src/mediafile/ImageFile.cxx
    src/mediafile/MediaFile.cxx
    src/mediafile/OggAudioFile.cxx
    src/mediafile/PNGImageFile.cxx
    src/mediafile/SGIImageFile.cxx
    src/mediafile/WaveAudioFile.cxx
)

set(DATE_SRCS
    src/date/buildDate.cxx
)

# BZFlag client source files
set(BZFLAG_SRCS
    src/bzflag/ActionBinding.cxx
    src/bzflag/AutoPilot.cxx
    src/bzflag/BackgroundRenderer.cxx
    src/bzflag/BaseLocalPlayer.cxx
    src/bzflag/CacheMenu.cxx
    src/bzflag/clientConfig.cxx
    src/bzflag/CommandsImplementation.cxx
    src/bzflag/ComposeDefaultKey.cxx
    src/bzflag/ControlPanel.cxx
    src/bzflag/defaultBZDB.cxx
    src/bzflag/DisplayMenu.cxx
    src/bzflag/Downloads.cxx
    src/bzflag/EffectsMenu.cxx
    src/bzflag/effectsRenderer.cxx
    src/bzflag/EntryZone.cxx
    src/bzflag/FlashClock.cxx
    src/bzflag/ForceFeedback.cxx
    src/bzflag/FormatMenu.cxx
    src/bzflag/GuidedMissleStrategy.cxx
    src/bzflag/GUIOptionsMenu.cxx
    src/bzflag/HelpMenu.cxx
    src/bzflag/HUDDialog.cxx
    src/bzflag/HUDDialogStack.cxx
    src/bzflag/HUDRenderer.cxx
    src/bzflag/HUDui.cxx
    src/bzflag/HUDuiControl.cxx
    src/bzflag/HUDuiDefaultKey.cxx
    src/bzflag/HUDuiJSTestLabel.cxx
    src/bzflag/HUDuiLabel.cxx
    src/bzflag/HUDuiList.cxx
    src/bzflag/HUDuiTextureLabel.cxx
    src/bzflag/HUDuiTypeIn.cxx
    src/bzflag/JoystickTestMenu.cxx
    src/bzflag/JoinMenu.cxx
    src/bzflag/InputMenu.cxx
    src/bzflag/KeyboardMapMenu.cxx
    src/bzflag/LocalCommand.cxx
    src/bzflag/LocalPlayer.cxx
    src/bzflag/MainMenu.cxx
    src/bzflag/MainWindow.cxx
    src/bzflag/MenuDefaultKey.cxx
    src/bzflag/motd.cxx
    src/bzflag/NewVersionMenu.cxx
    src/bzflag/OptionsMenu.cxx
    src/bzflag/Player.cxx
    src/bzflag/Plan.cxx
    src/bzflag/QuickKeysMenu.cxx
    src/bzflag/QuitMenu.cxx
    src/bzflag/RadarRenderer.cxx
    src/bzflag/Region.cxx
    src/bzflag/RegionPriorityQueue.cxx
    src/bzflag/RemotePlayer.cxx
    src/bzflag/Roaming.cxx
    src/bzflag/RobotPlayer.cxx
    src/bzflag/RoofTops.cxx
    src/bzflag/Roster.cxx
    src/bzflag/SaveWorldMenu.cxx
    src/bzflag/SceneBuilder.cxx
    src/bzflag/SceneRenderer.cxx
    src/bzflag/ScoreboardRenderer.cxx
    src/bzflag/SegmentedShotStrategy.cxx
    src/bzflag/ServerCommandKey.cxx
    src/bzflag/ServerLink.cxx
    src/bzflag/ServerListFilter.cxx
    src/bzflag/ServerListFilterHelpMenu.cxx
    src/bzflag/ServerListFilterMenu.cxx
    src/bzflag/ServerMenu.cxx
    src/bzflag/ServerStartMenu.cxx
    src/bzflag/ShockWaveStrategy.cxx
    src/bzflag/ShotPath.cxx
    src/bzflag/ShotPathSegment.cxx
    src/bzflag/ShotStats.cxx
    src/bzflag/ShotStatsDefaultKey.cxx
    src/bzflag/ShotStatistics.cxx
    src/bzflag/ShotStrategy.cxx
    src/bzflag/SilenceDefaultKey.cxx
    src/bzflag/TargetingUtils.cxx
    src/bzflag/TrackMarks.cxx
    src/bzflag/Weapon.cxx
    src/bzflag/WeatherRenderer.cxx
    src/bzflag/World.cxx
    src/bzflag/WorldBuilder.cxx
    src/bzflag/WorldPlayer.cxx
    src/bzflag/bzflag.cxx
    src/bzflag/callbacks.cxx
    src/bzflag/clientCommands.cxx
    src/bzflag/daylight.cxx
    src/bzflag/playing.cxx
    src/bzflag/sound.cxx
    src/bzflag/stars.cxx
)

# Create static libraries for the sub-components
add_library(bzcommon STATIC ${COMMON_SRCS})
target_include_directories(bzcommon PUBLIC ${CMAKE_SOURCE_DIR}/include)

add_library(bzgame STATIC ${GAME_SRCS})
target_include_directories(bzgame PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(bzgame PUBLIC bzcommon)

add_library(bznet STATIC ${NET_SRCS})
target_include_directories(bznet PUBLIC ${CMAKE_SOURCE_DIR}/include)

add_library(bzobstacle STATIC ${OBSTACLE_SRCS})
target_include_directories(bzobstacle PUBLIC ${CMAKE_SOURCE_DIR}/include)

add_library(bzplatform STATIC ${PLATFORM_SRCS})
target_include_directories(bzplatform PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(bzplatform PUBLIC SDL2::SDL2 SDL2::SDL2main)

add_library(bzogl STATIC ${OGL_SRCS})
target_include_directories(bzogl PUBLIC ${CMAKE_SOURCE_DIR}/include)

add_library(bzscene STATIC ${SCENE_SRCS})
target_include_directories(bzscene PUBLIC ${CMAKE_SOURCE_DIR}/include)

add_library(bzgeometry STATIC ${GEOMETRY_SRCS})
target_include_directories(bzgeometry PUBLIC ${CMAKE_SOURCE_DIR}/include)

add_library(bz3d STATIC ${THREED_SRCS})
target_include_directories(bz3d PUBLIC ${CMAKE_SOURCE_DIR}/include)

add_library(bzmediafile STATIC ${MEDIAFILE_SRCS})
target_include_directories(bzmediafile PUBLIC ${CMAKE_SOURCE_DIR}/include)

add_library(bzdate STATIC ${DATE_SRCS})
target_include_directories(bzdate PUBLIC ${CMAKE_SOURCE_DIR}/include)

# BZFlag client executable
add_executable(bzflag WIN32
    ${BZFLAG_SRCS}
    MSVC/bzflag.rc
)

# Windows resource file
if(WIN32)
    # Configure resource with proper icon path and include dir for resource.h
    set_source_files_properties(MSVC/bzflag.rc PROPERTIES
        INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/MSVC"
        OBJECT_DEPENDS "${CMAKE_SOURCE_DIR}/MSVC/resource.h;${CMAKE_SOURCE_DIR}/MSVC/bzflag.ico"
    )
endif()

target_link_libraries(bzflag PRIVATE
    bzcommon bzgame bznet bzobstacle bzplatform
    bzogl bzscene bzgeometry bz3d bzmediafile bzdate
    SDL2::SDL2 SDL2::SDL2main
    GLEW::GLEW OpenGL::GL
    CURL::libcurl c-ares::cares ZLIB::ZLIB
    ws2_32 dsound winmm glu32 opengl32
)

set_target_properties(bzflag PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Copy data directory
add_custom_command(TARGET bzflag POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/data $<TARGET_FILE_DIR:bzflag>/data
    COMMENT "Copying data files"
)

# Copy DLLs to output
if(WIN32)
    add_custom_command(TARGET bzflag POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL2::SDL2> $<TARGET_FILE_DIR:bzflag>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:CURL::libcurl> $<TARGET_FILE_DIR:bzflag>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:c-ares::cares> $<TARGET_FILE_DIR:bzflag>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:ZLIB::ZLIB> $<TARGET_FILE_DIR:bzflag>
        COMMENT "Copying runtime DLLs"
    )
endif()