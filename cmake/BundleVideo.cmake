# Package the example from a fresh staging directory; never modify installed libraries.
cmake_minimum_required(VERSION 3.16)
if(NOT APPLE OR NOT EXISTS "${EXECUTABLE}" OR IS_DIRECTORY "${EXECUTABLE}")
    message(FATAL_ERROR "BundleVideo requires macOS and -DEXECUTABLE=<built video executable>")
endif()
include(BundleUtilities)
find_program(CODESIGN codesign)
if(NOT CODESIGN)
    message(FATAL_ERROR "codesign is required to package video.app")
endif()
get_filename_component(EXECUTABLE "${EXECUTABLE}" ABSOLUTE)
set(bundle "${EXECUTABLE}.app")
set(staging "${EXECUTABLE}.bundle-staging")
set(app "${staging}/video.app")
file(REMOVE_RECURSE "${staging}")
file(MAKE_DIRECTORY "${app}/Contents/MacOS" "${app}/Contents/Resources/licenses")
file(COPY "${EXECUTABLE}" DESTINATION "${app}/Contents/MacOS")
get_filename_component(name "${EXECUTABLE}" NAME)
file(WRITE "${app}/Contents/Info.plist" "<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">
<plist version=\"1.0\"><dict>
<key>CFBundleExecutable</key>
<string>${name}</string>
<key>CFBundleIdentifier</key><string>org.better-c-ui.video</string>
<key>CFBundleName</key><string>Video player</string>
<key>CFBundlePackageType</key><string>APPL</string>
<key>CFBundleShortVersionString</key><string>${VERSION}</string>
<key>CFBundleVersion</key><string>${VERSION}</string>
<key>NSHighResolutionCapable</key><true/>
</dict></plist>
")
get_bundle_and_executable("${app}" unused executable valid)
if(NOT valid)
    message(FATAL_ERROR "Could not create the application bundle")
endif()

# Resolve transitive @rpath dependencies before BundleUtilities flattens them.
file(GET_RUNTIME_DEPENDENCIES EXECUTABLES "${EXECUTABLE}"
    RESOLVED_DEPENDENCIES_VAR dependencies UNRESOLVED_DEPENDENCIES_VAR missing
    POST_EXCLUDE_REGEXES "^/System/" "^/usr/lib/")
if(missing)
    message(FATAL_ERROR "Unresolved video runtime libraries: ${missing}")
endif()
set(library_dirs)
set(license_roots)
foreach(dependency IN LISTS dependencies)
    get_filename_component(directory "${dependency}" DIRECTORY)
    list(APPEND library_dirs "${directory}")
    get_filename_component(source "${dependency}" REALPATH)
    if(source MATCHES "^(.*/Cellar/[^/]+/[^/]+)/")
        list(APPEND license_roots "${CMAKE_MATCH_1}")
    endif()
endforeach()
list(REMOVE_DUPLICATES library_dirs)
list(REMOVE_DUPLICATES license_roots)
foreach(root IN LISTS license_roots)
    get_filename_component(version "${root}" NAME)
    get_filename_component(package "${root}" DIRECTORY)
    get_filename_component(package "${package}" NAME)
    file(GLOB notices LIST_DIRECTORIES false "${root}/LICENSE*" "${root}/COPYING*" "${root}/Copyright*" "${root}/COPYRIGHT*" "${root}/NOTICE*")
    if(notices)
        file(COPY ${notices} DESTINATION "${app}/Contents/Resources/licenses/${package}-${version}")
    endif()
endforeach()
get_filename_component(repo "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
file(COPY "${repo}/LICENSE" "${repo}/THIRD_PARTY.md" DESTINATION "${app}/Contents/Resources/licenses/cui")
file(COPY "${repo}/assets/fonts/LICENSE-Inter.txt" "${repo}/assets/fonts/LICENSE-Geist.txt"
    "${repo}/assets/fonts/LICENSE-Lucide.txt" "${repo}/third_party/LICENSE-Unicode.txt"
    DESTINATION "${app}/Contents/Resources/licenses/cui")

set(BU_CHMOD_BUNDLE_ITEMS TRUE)
fixup_bundle("${app}" "" "${library_dirs}")
# Rewriting Mach-O load paths invalidates signatures, including on Apple Silicon.
file(GLOB libraries "${app}/Contents/Frameworks/*.dylib")
foreach(library IN LISTS libraries)
    execute_process(COMMAND "${CODESIGN}" --force --sign - "${library}" RESULT_VARIABLE result ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Signing ${library}: ${error}")
    endif()
endforeach()
execute_process(COMMAND "${CODESIGN}" --force --sign - "${app}" RESULT_VARIABLE result ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Signing video.app: ${error}")
endif()
execute_process(COMMAND "${CODESIGN}" --verify --deep --strict "${app}" RESULT_VARIABLE result ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Bundle signature verification: ${error}")
endif()
file(REMOVE_RECURSE "${bundle}")
file(RENAME "${app}" "${bundle}")
file(REMOVE_RECURSE "${staging}")
message(STATUS "Self-contained video player: ${bundle}")
