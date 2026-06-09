cmake_minimum_required(VERSION 3.19)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(PROJECT_NAME "PetForDesktop")
set(PROJECT_VERSION 1.4.1)

set(REL_RESOURCES_DIR "content")
set(ABS_RESOURCE_DIR "${CMAKE_SOURCE_DIR}/${REL_RESOURCES_DIR}")
set(ABS_DEPS_DIR "${CMAKE_SOURCE_DIR}/deps")
set(ABS_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
set(ABS_SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(ABS_MAIN_FILE "${ABS_SRC_DIR}/main.cpp")
set(ABS_RESOURCES_BUILD_LINK "${CMAKE_CURRENT_BINARY_DIR}/${REL_RESOURCES_DIR}")
set(ABS_LICENSE_PATH "${CMAKE_SOURCE_DIR}/LICENSE")
set(ABS_README_PATH "${CMAKE_SOURCE_DIR}/README.md")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

project(${PROJECT_NAME} VERSION ${PROJECT_VERSION} LANGUAGES C CXX)
message(STATUS "Project ${PROJECT_NAME} ${PROJECT_VERSION}")

include(FetchContent)
include(GNUInstallDirs)

option(USE_OPENGL_API "Use OpenGL graphic api" FALSE)
option(PETFORDESKTOP_USE_FETCHCONTENT_DEPS "Fetch dependencies when bundled submodules are unavailable" OFF)

message(STATUS "USE_OPENGL_API: ${USE_OPENGL_API}")
message(STATUS "PETFORDESKTOP_USE_FETCHCONTENT_DEPS: ${PETFORDESKTOP_USE_FETCHCONTENT_DEPS}")

function(pet_resolve_target output_var)
    foreach(candidate IN LISTS ARGN)
        if(TARGET ${candidate})
            set(${output_var} ${candidate} PARENT_SCOPE)
            return()
        endif()
    endforeach()

    message(FATAL_ERROR "Unable to resolve a dependency target for ${output_var}")
endfunction()

function(pet_add_dependency dep_name local_path git_repo git_tag)
    if(EXISTS "${local_path}/CMakeLists.txt")
        add_subdirectory("${local_path}" "${CMAKE_BINARY_DIR}/_deps/${dep_name}")
        return()
    endif()

    if(PETFORDESKTOP_USE_FETCHCONTENT_DEPS)
        FetchContent_Declare(
            ${dep_name}
            GIT_REPOSITORY ${git_repo}
            GIT_TAG ${git_tag}
            GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(${dep_name})
        return()
    endif()

    message(FATAL_ERROR
        "Missing dependency '${dep_name}' at '${local_path}'. "
        "Enable PETFORDESKTOP_USE_FETCHCONTENT_DEPS to fetch it explicitly.")
endfunction()

file(GLOB_RECURSE project_source_files LIST_DIRECTORIES false CONFIGURE_DEPENDS src/*.cpp src/*.c)
file(GLOB_RECURSE project_headers LIST_DIRECTORIES false CONFIGURE_DEPENDS include/*.hpp include/*.h)
message(STATUS "Headers: ${project_headers}")
message(STATUS "Sources : ${project_source_files}")

if (WIN32)
    set(APP_ICON_RESOURCE "${CMAKE_CURRENT_SOURCE_DIR}/appicon.rc")
endif ()

add_executable(${PROJECT_NAME} ${APP_ICON_RESOURCE} ${project_headers} ${project_source_files})

if (WIN32 AND CMAKE_BUILD_TYPE MATCHES Release)
    set(SUB_SYS WIN32)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
else()
    set(SUB_SYS "")
endif()

if (SUB_SYS)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

pet_add_dependency(glfw "${ABS_DEPS_DIR}/glfw" "https://github.com/glfw/glfw.git" "b00e6a8a88ad1b60c0a045e696301deb92c9a13e")
pet_add_dependency(yaml-cpp "${ABS_DEPS_DIR}/yaml-cpp" "https://github.com/jbeder/yaml-cpp.git" "2decf96e915d2b0c26c68c1659665789dfef2633")
set(CPR_CURL_USE_LIBPSL OFF CACHE BOOL "" FORCE)
set(CPR_USE_SYSTEM_LIB_PSL OFF CACHE BOOL "" FORCE)
pet_add_dependency(cpr "${ABS_DEPS_DIR}/cpr" "https://github.com/libcpr/cpr.git" "e3edc338eb74312b92186b93039e29bb04a6d4f8")
pet_add_dependency(Boxer "${ABS_DEPS_DIR}/Boxer" "https://github.com/aaronmjacobs/Boxer.git" "5561c0fa1262f9b182a56cd0b06bcdca84a029a6")

set(IMGUI_IMPL_OPENGL3 ON)
set(IMGUI_IMPL_GLFW ON)
set(IMGUI_EXAMPLES OFF)
set(IMGUI_DEMO OFF)
set(IMGUI_ENABLE_FREETYPE OFF)
set(IMGUI_IMPL_SDL OFF)
set(IMGUI_IMPL_METAL OFF)
set(IMGUI_IMPL_OSX OFF)
set(IMGUI_IMPL_WIN32 OFF)
set(IMGUI_IMPL_GLUT OFF)
set(IMGUI_IMPL_OPENGL2 OFF)
set(IMGUI_IMPL_DX9 OFF)
set(IMGUI_IMPL_DX10 OFF)
set(IMGUI_IMPL_DX11 OFF)
set(IMGUI_IMPL_DX12 OFF)
set(IMGUI_IMPL_VULKAN OFF)

set(PET_IMGUI_ROOT_DIR "${ABS_DEPS_DIR}/imgui/imgui")
if(EXISTS "${PET_IMGUI_ROOT_DIR}/imgui.h")
    add_subdirectory("${ABS_DEPS_DIR}/imgui")
elseif(PETFORDESKTOP_USE_FETCHCONTENT_DEPS)
    set(PET_IMGUI_ROOT_DIR "${CMAKE_BINARY_DIR}/_deps/imgui-src")
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.92.8-docking
        GIT_SHALLOW TRUE
        SOURCE_DIR "${PET_IMGUI_ROOT_DIR}")
    FetchContent_Populate(imgui)
    configure_file("${ABS_DEPS_DIR}/imgui/CMakeLists.txt" "${PET_IMGUI_ROOT_DIR}/CMakeLists.txt" COPYONLY)
    add_subdirectory("${PET_IMGUI_ROOT_DIR}")
else()
    message(FATAL_ERROR
        "Missing Dear ImGui sources at '${PET_IMGUI_ROOT_DIR}'. "
        "Enable PETFORDESKTOP_USE_FETCHCONTENT_DEPS to fetch it explicitly.")
endif()

pet_resolve_target(PET_GLFW_TARGET glfw glfw3::glfw3)
pet_resolve_target(PET_YAMLCPP_TARGET yaml-cpp yaml-cpp::yaml-cpp)
pet_resolve_target(PET_CPR_TARGET cpr cpr::cpr)
pet_resolve_target(PET_BOXER_TARGET Boxer Boxer::Boxer)

set(PET_IMGUI_TARGETS imgui imgui-glfw imgui-opengl3)
foreach(target_name IN LISTS PET_IMGUI_TARGETS)
    if(NOT TARGET ${target_name})
        message(FATAL_ERROR "Missing imgui target '${target_name}'. The bundled imgui wrapper is required.")
    endif()
endforeach()

target_link_libraries(${PROJECT_NAME} PRIVATE ${PET_GLFW_TARGET} ${PET_BOXER_TARGET} ${PET_YAMLCPP_TARGET} ${PET_CPR_TARGET}
                                              imgui imgui-glfw imgui-opengl3)

target_include_directories(${PROJECT_NAME} PRIVATE "${ABS_INCLUDE_DIR}" "${ABS_SRC_DIR}" "${PET_IMGUI_ROOT_DIR}")

if(USE_OPENGL_API)
    find_package(OpenGL REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${OPENGL_LIBRARIES})
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE RESOURCE_PATH="${REL_RESOURCES_DIR}")
target_compile_definitions(${PROJECT_NAME} PRIVATE PROJECT_NAME="${PROJECT_NAME}")
target_compile_definitions(${PROJECT_NAME} PRIVATE PROJECT_VERSION="${PROJECT_VERSION}")
target_compile_definitions(${PROJECT_NAME} PRIVATE IMGUI_DEFINE_MATH_OPERATORS)

if(USE_OPENGL_API)
    target_compile_definitions(${PROJECT_NAME} PRIVATE SHADER_RESOURCE_PATH="${REL_RESOURCES_DIR}/shader/glsl")
    target_compile_definitions(${PROJECT_NAME} PRIVATE SHADER_VERTEX_EXT=".vs")
    target_compile_definitions(${PROJECT_NAME} PRIVATE SHADER_FRAG_EXT=".fs")
    target_compile_definitions(${PROJECT_NAME} PRIVATE GLFW_INCLUDE_NONE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_OPENGL_API)
endif()

if (WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE WIN32_LEAN_AND_MEAN)
    target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX)
endif ()

add_custom_target(copy_assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_LIST_DIR}/${REL_RESOURCES_DIR} ${CMAKE_CURRENT_BINARY_DIR}/${REL_RESOURCES_DIR}
)
add_dependencies(${PROJECT_NAME} copy_assets)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY ${REL_RESOURCES_DIR} DESTINATION ${CMAKE_INSTALL_DATADIR})
install(FILES ${ABS_LICENSE_PATH} ${ABS_README_PATH} DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME})

set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_VENDOR "Foxland")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A fox companion like you've never dreamed")
set(CPACK_PACKAGE_INSTALL_DIRECTORY ${PROJECT_NAME})
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)

if(WIN32)
    set(CPACK_GENERATOR "ZIP;NSIS")
    set(CPACK_NSIS_DISPLAY_NAME ${PROJECT_NAME})
    set(CPACK_NSIS_PACKAGE_NAME ${PROJECT_NAME})
    set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/Renardjojo/PetDesktop")
else()
    set(CPACK_GENERATOR "ZIP;TGZ")
endif()

include(CPack)

enable_testing()

add_executable(pet-p2-tests
    tests/p2_regression_tests.cpp
    src/FileExplorer.cpp
    src/PlatformServices.cpp
    src/PetLogic.cpp
    src/Settings.cpp
    src/MonitorTopologyCache.cpp
    src/Updater.cpp
)

target_include_directories(pet-p2-tests PRIVATE ${ABS_INCLUDE_DIR} ${ABS_SRC_DIR} ${PET_IMGUI_ROOT_DIR})
target_link_libraries(pet-p2-tests PRIVATE ${PET_BOXER_TARGET} ${PET_YAMLCPP_TARGET} ${PET_CPR_TARGET} ${PET_GLFW_TARGET})

target_compile_definitions(pet-p2-tests PRIVATE PET_P2_TESTS)
target_compile_definitions(pet-p2-tests PRIVATE PROJECT_NAME="${PROJECT_NAME}")
target_compile_definitions(pet-p2-tests PRIVATE PROJECT_VERSION="${PROJECT_VERSION}")
target_compile_definitions(pet-p2-tests PRIVATE RESOURCE_PATH="${REL_RESOURCES_DIR}")

if(WIN32)
    target_compile_definitions(pet-p2-tests PRIVATE WIN32_LEAN_AND_MEAN)
    target_compile_definitions(pet-p2-tests PRIVATE NOMINMAX)
endif()

add_test(NAME pet-p2-regression COMMAND pet-p2-tests)
set_tests_properties(pet-p2-regression PROPERTIES TIMEOUT 600)

set(PET_P2_REGRESSION_TEST_NAMES
    monitor_scaling_transform
    monitor_fallback_and_selection
    settings_validation_hardening
    update_metadata_validation
    cursor_normalization_for_mixed_scale
    cursor_normalization_for_scale_variants
    state_machine_and_transition_helpers
    physics_motion_drag_and_collision_states
    pause_resume_and_release_velocity
    seeded_walk_and_jump_golden_outcomes
    cursor_delta_pruning
)

foreach(test_name IN LISTS PET_P2_REGRESSION_TEST_NAMES)
    add_test(NAME "pet-p2-${test_name}" COMMAND pet-p2-tests "${test_name}")
    set_tests_properties("pet-p2-${test_name}" PROPERTIES TIMEOUT 120)
endforeach()
