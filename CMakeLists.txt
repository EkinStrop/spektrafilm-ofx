cmake_minimum_required(VERSION 3.24)

project(SpektraFilmOFX LANGUAGES CXX OBJCXX)

set(SPEKTRAFILM_VERSION "0.1.0")

set(_SPEKTRAFILM_VENV_PYTHON "${CMAKE_CURRENT_SOURCE_DIR}/../../.venv/bin/python")
if(EXISTS "${_SPEKTRAFILM_VENV_PYTHON}" AND NOT DEFINED Python3_EXECUTABLE)
  set(Python3_EXECUTABLE "${_SPEKTRAFILM_VENV_PYTHON}" CACHE FILEPATH "Python interpreter for SpektraFilm data generation")
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(PNG REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_STANDARD 17)
set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)

if(NOT APPLE)
  message(FATAL_ERROR "SpektraFilm OFX currently targets macOS because the renderer is Metal-first.")
endif()

set(SPEKTRAFILM_OFX_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/openfx" CACHE PATH "Vendored or external OpenFX SDK root")
set(SPEKTRAFILM_OFX_HEADERS "" CACHE PATH "Directory containing OpenFX API headers such as ofxImageEffect.h")
if(NOT SPEKTRAFILM_OFX_HEADERS)
  if(DEFINED ENV{OFX_HEADER_DIR})
    set(SPEKTRAFILM_OFX_HEADERS "$ENV{OFX_HEADER_DIR}")
  else()
    set(SPEKTRAFILM_OFX_HEADERS "${SPEKTRAFILM_OFX_ROOT}/include")
  endif()
endif()

set(SPEKTRAFILM_OFX_SUPPORT_HEADERS "${SPEKTRAFILM_OFX_ROOT}/Support" CACHE PATH "Directory containing OpenFX plugin support headers")
set(SPEKTRAFILM_OFX_HOST_SUPPORT_HEADERS "${SPEKTRAFILM_OFX_ROOT}/HostSupport" CACHE PATH "Directory containing OpenFX host support headers")
set(SPEKTRAFILM_OFX_EXAMPLE_HEADERS "${SPEKTRAFILM_OFX_ROOT}/Examples/include" CACHE PATH "Directory containing OpenFX example utility headers")

set(SPEKTRAFILM_GENERATED_PROFILE_CURVES "${CMAKE_CURRENT_BINARY_DIR}/generated/SpektraGeneratedProfileCurves.cpp")
set(SPEKTRAFILM_GENERATED_PROFILE_COUNTS "${CMAKE_CURRENT_BINARY_DIR}/generated/SpektraGeneratedProfileCounts.h")
set(SPEKTRAFILM_GENERATED_HANATOS_LUT "${CMAKE_CURRENT_BINARY_DIR}/generated/SpektraHanatos2025Spectra.f32")
set(SPEKTRAFILM_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Resources/data")
set(SPEKTRAFILM_HANATOS_LUT_DATA "${SPEKTRAFILM_DATA_DIR}/luts/spectral_upsampling/irradiance_xy_tc.npy")
file(GLOB SPEKTRAFILM_PROFILE_JSON CONFIGURE_DEPENDS
  "${SPEKTRAFILM_DATA_DIR}/profiles/*.json"
)
file(GLOB_RECURSE SPEKTRAFILM_FILTER_DATA CONFIGURE_DEPENDS
  "${SPEKTRAFILM_DATA_DIR}/filters/*"
)
file(GLOB_RECURSE SPEKTRAFILM_STANDARD_DATA CONFIGURE_DEPENDS
  "${SPEKTRAFILM_DATA_DIR}/standards/*"
)
file(GLOB_RECURSE SPEKTRAFILM_LUT_DATA CONFIGURE_DEPENDS
  "${SPEKTRAFILM_DATA_DIR}/luts/*"
)

add_custom_command(
  OUTPUT
          "${SPEKTRAFILM_GENERATED_PROFILE_CURVES}"
          "${SPEKTRAFILM_GENERATED_PROFILE_COUNTS}"
          "${SPEKTRAFILM_GENERATED_HANATOS_LUT}"
  COMMAND "${CMAKE_COMMAND}" -E env "SPEKTRAFILM_DATA_DIR=${SPEKTRAFILM_DATA_DIR}"
          "${Python3_EXECUTABLE}"
          "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_profile_curves.py"
          "--output"
          "${SPEKTRAFILM_GENERATED_PROFILE_CURVES}"
          "--counts-output"
          "${SPEKTRAFILM_GENERATED_PROFILE_COUNTS}"
          "--hanatos-output"
          "${SPEKTRAFILM_GENERATED_HANATOS_LUT}"
  DEPENDS
          "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_profile_curves.py"
          "${CMAKE_CURRENT_SOURCE_DIR}/tools/ofx_stock_lists.py"
          "${SPEKTRAFILM_HANATOS_LUT_DATA}"
          ${SPEKTRAFILM_PROFILE_JSON}
          ${SPEKTRAFILM_FILTER_DATA}
          ${SPEKTRAFILM_STANDARD_DATA}
          ${SPEKTRAFILM_LUT_DATA}
  VERBATIM
)

set(SPEKTRAFILM_PLUGIN_SOURCES
  src/SpektraFilmPlugin.cpp
  src/SpektraMetalRenderer.mm
  "${SPEKTRAFILM_GENERATED_PROFILE_CURVES}"
  "${SPEKTRAFILM_GENERATED_PROFILE_COUNTS}"
)

set(SPEKTRAFILM_METAL_AIR "${CMAKE_CURRENT_BINARY_DIR}/SpektraFilm.air")
set(SPEKTRAFILM_METALLIB "${CMAKE_CURRENT_BINARY_DIR}/SpektraFilm.metallib")
set(SPEKTRAFILM_METAL_MODULE_CACHE "${CMAKE_CURRENT_BINARY_DIR}/metal-module-cache")

add_custom_command(
  OUTPUT "${SPEKTRAFILM_METAL_AIR}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${SPEKTRAFILM_METAL_MODULE_CACHE}"
  COMMAND xcrun -sdk macosx metal
          -std=macos-metal2.4
          -fmodules-cache-path="${SPEKTRAFILM_METAL_MODULE_CACHE}"
          -c "${CMAKE_CURRENT_SOURCE_DIR}/shaders/SpektraFilm.metal"
          -o "${SPEKTRAFILM_METAL_AIR}"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/SpektraFilm.metal"
  VERBATIM
)

add_custom_command(
  OUTPUT "${SPEKTRAFILM_METALLIB}"
  COMMAND xcrun -sdk macosx metallib
          "${SPEKTRAFILM_METAL_AIR}"
          -o "${SPEKTRAFILM_METALLIB}"
  DEPENDS "${SPEKTRAFILM_METAL_AIR}"
  VERBATIM
)

add_custom_target(SpektraFilmMetalLibrary ALL DEPENDS "${SPEKTRAFILM_METALLIB}")

set(SPEKTRAFILM_WEBSITE_DOWNLOAD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../website/public/downloads")
set(SPEKTRAFILM_USER_MANUAL_PDF "${CMAKE_CURRENT_SOURCE_DIR}/../../docs/user_manual/main.pdf")
set(SPEKTRA_OFX_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/Legal/SPEKTRA_OFX_LICENSE.txt")
set(SPEKTRA_OFX_LUT_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/Legal/SPEKTRA_OFX_LUT_LICENSE.txt")
set(SPEKTRA_OFX_THIRD_PARTY_NOTICES "${CMAKE_CURRENT_SOURCE_DIR}/Legal/THIRD_PARTY_NOTICES.txt")

function(add_spektra_ofx_plugin target label plugin_id flavor package_for_website)
  set(SPEKTRAFILM_BUNDLE_EXECUTABLE "${label}.ofx")
  set(SPEKTRAFILM_BUNDLE_IDENTIFIER "${plugin_id}")
  set(SPEKTRAFILM_BUNDLE_NAME "${label}")
  set(SPEKTRAFILM_PLUGIN_ICON_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icons/${label}.png")
  set(SPEKTRAFILM_RESOLVE_ICON_NAME "${plugin_id}.png")
  set(SPEKTRAFILM_MANIFEST_ID "${plugin_id}")
  set(SPEKTRAFILM_MANIFEST_NAME "${label}")
  set(SPEKTRAFILM_MANIFEST_VERSION "${SPEKTRAFILM_VERSION}")

  set(info_plist "${CMAKE_CURRENT_BINARY_DIR}/${target}-Info.plist")
  set(plugin_manifest "${CMAKE_CURRENT_BINARY_DIR}/${target}-plugin_manifest.json")
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Resources/Info.plist.in" "${info_plist}" @ONLY)
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Resources/plugin_manifest.json.in" "${plugin_manifest}" @ONLY)

  add_library(${target} MODULE ${SPEKTRAFILM_PLUGIN_SOURCES})

  target_include_directories(${target} PRIVATE
    "${SPEKTRAFILM_OFX_HEADERS}"
    "${SPEKTRAFILM_OFX_SUPPORT_HEADERS}"
    "${SPEKTRAFILM_OFX_HOST_SUPPORT_HEADERS}"
    "${SPEKTRAFILM_OFX_EXAMPLE_HEADERS}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
  )

  target_compile_definitions(${target} PRIVATE
    SPEKTRAFILM_VERSION_STRING="${SPEKTRAFILM_VERSION}"
    SPEKTRAFILM_PLUGIN_IDENTIFIER="${plugin_id}"
    SPEKTRAFILM_PLUGIN_LABEL="${label}"
    SPEKTRAFILM_PLUGIN_FLAVOR=${flavor}
  )

  target_compile_options(${target} PRIVATE
    "$<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>"
  )

  target_link_libraries(${target} PRIVATE
    "-framework ApplicationServices"
    "-framework Foundation"
    "-framework Metal"
    "-framework MetalKit"
    "-framework QuartzCore"
  )

  set_target_properties(${target} PROPERTIES
    BUNDLE TRUE
    BUNDLE_EXTENSION "ofx.bundle"
    MACOSX_BUNDLE_INFO_PLIST "${info_plist}"
    OUTPUT_NAME "${label}"
    PREFIX ""
  )

  set(bundle_resource_deps
    "${SPEKTRAFILM_METALLIB}"
    "${plugin_manifest}"
    "${SPEKTRAFILM_GENERATED_HANATOS_LUT}"
    "${SPEKTRAFILM_USER_MANUAL_PDF}"
    "${SPEKTRA_OFX_LICENSE}"
    "${SPEKTRA_OFX_LUT_LICENSE}"
    "${SPEKTRA_OFX_THIRD_PARTY_NOTICES}"
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icons/SpektraFlow.svg"
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icons/SpektraPro.svg"
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icons/SpektraFilm.svg"
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icons/SpektraFlow.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icons/SpektraPro.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icons/SpektraFilm.png"
    "${SPEKTRAFILM_PLUGIN_ICON_SOURCE}"
  )
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/Legal"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/icons"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:${target}>"
            "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/MacOS/${label}.ofx"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${SPEKTRAFILM_METALLIB}"
            "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/SpektraFilm.metallib"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${plugin_manifest}"
            "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/plugin_manifest.json"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${SPEKTRAFILM_GENERATED_HANATOS_LUT}"
            "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/SpektraHanatos2025Spectra.f32"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${SPEKTRAFILM_USER_MANUAL_PDF}"
            "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/SpektraFilm_User_Manual.pdf"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${SPEKTRA_OFX_LICENSE}"
            "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/Legal/SPEKTRA_OFX_LICENSE.txt"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${SPEKTRA_OFX_LUT_LICENSE}"
            "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/Legal/SPEKTRA_OFX_LUT_LICENSE.txt"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${SPEKTRA_OFX_THIRD_PARTY_NOTICES}"
            "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/Legal/THIRD_PARTY_NOTICES.txt"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icons/SpektraFlow.svg"
            "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/icons/SpektraFlow.svg"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icons/SpektraPro.svg"
            "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/icons/SpektraPro.svg"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icons/SpektraFilm.svg"
            "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/icons/SpektraFilm.svg"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icons/SpektraFlow.png"
            "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/icons/SpektraFlow.png"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icons/SpektraPro.png"
            "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/icons/SpektraPro.png"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icons/SpektraFilm.png"
            "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/icons/SpektraFilm.png"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${SPEKTRAFILM_PLUGIN_ICON_SOURCE}"
            "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/${SPEKTRAFILM_RESOLVE_ICON_NAME}"
    VERBATIM
  )
  add_custom_target(${target}BundleResources ALL
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/Legal"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/icons"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:${target}>"
            "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/MacOS/${label}.ofx"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${SPEKTRAFILM_METALLIB}"
            "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/SpektraFilm.metallib"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${plugin_manifest}"
            "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/plugin_manifest.json"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${SPEKTRAFILM_GENERATED_HANATOS_LUT}"
            "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/SpektraHanatos2025Spectra.f32"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${SPEKTRAFILM_USER_MANUAL_PDF}"
            "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/SpektraFilm_User_Manual.pdf"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${SPEKTRA_OFX_LICENSE}"
            "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/Legal/SPEKTRA_OFX_LICENSE.txt"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${SPEKTRA_OFX_LUT_LICENSE}"
            "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/Legal/SPEKTRA_OFX_LUT_LICENSE.txt"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${SPEKTRA_OFX_THIRD_PARTY_NOTICES}"
            "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/Legal/THIRD_PARTY_NOTICES.txt"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icons/SpektraFlow.svg"
            "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/icons/SpektraFlow.svg"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icons/SpektraPro.svg"
            "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/icons/SpektraPro.svg"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icons/SpektraFilm.svg"
            "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/icons/SpektraFilm.svg"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icons/SpektraFlow.png"
            "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/icons/SpektraFlow.png"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icons/SpektraPro.png"
            "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/icons/SpektraPro.png"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icons/SpektraFilm.png"
            "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/icons/SpektraFilm.png"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${SPEKTRAFILM_PLUGIN_ICON_SOURCE}"
            "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/${SPEKTRAFILM_RESOLVE_ICON_NAME}"
    DEPENDS ${bundle_resource_deps}
    VERBATIM
  )
  add_dependencies(${target}BundleResources ${target} SpektraFilmMetalLibrary)
  add_dependencies(${target} SpektraFilmMetalLibrary)

  if(package_for_website)
    set(download_zip "${SPEKTRAFILM_WEBSITE_DOWNLOAD_DIR}/${label}-OFX-macOS.zip")
    set(download_stage "${CMAKE_CURRENT_BINARY_DIR}/${target}-download")
    add_custom_target(${target}DownloadZip
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${SPEKTRAFILM_WEBSITE_DOWNLOAD_DIR}"
      COMMAND "${CMAKE_COMMAND}" -E rm -f "${download_zip}"
      COMMAND "${CMAKE_COMMAND}" -E rm -rf "${download_stage}"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${download_stage}"
      COMMAND "${CMAKE_COMMAND}" -E copy_directory
              "$<TARGET_BUNDLE_DIR:${target}>"
              "${download_stage}/${label}.ofx.bundle"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
              "${SPEKTRA_OFX_LICENSE}"
              "${download_stage}/SPEKTRA_OFX_LICENSE.txt"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
              "${SPEKTRA_OFX_LUT_LICENSE}"
              "${download_stage}/SPEKTRA_OFX_LUT_LICENSE.txt"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
              "${SPEKTRA_OFX_THIRD_PARTY_NOTICES}"
              "${download_stage}/THIRD_PARTY_NOTICES.txt"
      COMMAND "${CMAKE_COMMAND}" -E env COPYFILE_DISABLE=1 /usr/bin/ditto
              -c
              -k
              --norsrc
              "${download_stage}"
              "${download_zip}"
      DEPENDS ${target}BundleResources
      VERBATIM
    )
  endif()

  install(TARGETS ${target}
    LIBRARY DESTINATION "/Library/OFX/Plugins"
    BUNDLE DESTINATION "/Library/OFX/Plugins"
  )
endfunction()

add_spektra_ofx_plugin(SpektraFlow SpektraFlow org.spektrafilm.flow 0 TRUE)
add_spektra_ofx_plugin(SpektraPro SpektraPro org.spektrafilm.pro 1 TRUE)
add_spektra_ofx_plugin(SpektraFilm SpektraFilm org.spektrafilm.ofx 2 FALSE)

add_executable(SpektraFilmPerfHarness
  tools/SpektraMetalPerfHarness.mm
  src/SpektraMetalRenderer.mm
  "${SPEKTRAFILM_GENERATED_PROFILE_CURVES}"
  "${SPEKTRAFILM_GENERATED_PROFILE_COUNTS}"
)

add_executable(SpektraFilmEvaluationHarness
  tools/SpektraMetalEvaluationHarness.mm
  src/SpektraMetalRenderer.mm
  "${SPEKTRAFILM_GENERATED_PROFILE_CURVES}"
  "${SPEKTRAFILM_GENERATED_PROFILE_COUNTS}"
)

add_executable(SpektraVariantGenerator
  tools/SpektraVariantGenerator.mm
  src/SpektraMetalRenderer.mm
  "${SPEKTRAFILM_GENERATED_PROFILE_CURVES}"
  "${SPEKTRAFILM_GENERATED_PROFILE_COUNTS}"
)

target_include_directories(SpektraFilmPerfHarness PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "${CMAKE_CURRENT_BINARY_DIR}/generated"
)

target_include_directories(SpektraFilmEvaluationHarness PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "${CMAKE_CURRENT_BINARY_DIR}/generated"
)

target_include_directories(SpektraVariantGenerator PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "${CMAKE_CURRENT_BINARY_DIR}/generated"
)

target_compile_options(SpektraFilmPerfHarness PRIVATE
  "$<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>"
)

target_compile_options(SpektraFilmEvaluationHarness PRIVATE
  "$<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>"
)

target_compile_options(SpektraVariantGenerator PRIVATE
  "$<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>"
)

target_link_libraries(SpektraFilmPerfHarness PRIVATE
  "-framework Foundation"
  "-framework Metal"
  "-framework MetalKit"
  "-framework QuartzCore"
)

target_link_libraries(SpektraFilmEvaluationHarness PRIVATE
  "-framework Foundation"
  "-framework Metal"
  "-framework MetalKit"
  "-framework QuartzCore"
)

target_link_libraries(SpektraVariantGenerator PRIVATE
  PNG::PNG
  "-framework CoreGraphics"
  "-framework Foundation"
  "-framework ImageIO"
  "-framework Metal"
  "-framework MetalKit"
  "-framework QuartzCore"
)

add_dependencies(SpektraFilmPerfHarness SpektraFilmMetalLibrary)
add_dependencies(SpektraFilmEvaluationHarness SpektraFilmMetalLibrary)
add_dependencies(SpektraVariantGenerator SpektraFilmMetalLibrary)

add_custom_command(TARGET SpektraFilmPerfHarness POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
          "${SPEKTRAFILM_METALLIB}"
          "$<TARGET_FILE_DIR:SpektraFilmPerfHarness>/SpektraFilm.metallib"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
          "${SPEKTRAFILM_GENERATED_HANATOS_LUT}"
          "$<TARGET_FILE_DIR:SpektraFilmPerfHarness>/SpektraHanatos2025Spectra.f32"
  VERBATIM
)

add_custom_command(TARGET SpektraFilmEvaluationHarness POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
          "${SPEKTRAFILM_METALLIB}"
          "$<TARGET_FILE_DIR:SpektraFilmEvaluationHarness>/SpektraFilm.metallib"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
          "${SPEKTRAFILM_GENERATED_HANATOS_LUT}"
          "$<TARGET_FILE_DIR:SpektraFilmEvaluationHarness>/SpektraHanatos2025Spectra.f32"
  VERBATIM
)

add_custom_command(TARGET SpektraVariantGenerator POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
          "${SPEKTRAFILM_METALLIB}"
          "$<TARGET_FILE_DIR:SpektraVariantGenerator>/SpektraFilm.metallib"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
          "${SPEKTRAFILM_GENERATED_HANATOS_LUT}"
          "$<TARGET_FILE_DIR:SpektraVariantGenerator>/SpektraHanatos2025Spectra.f32"
  VERBATIM
)
