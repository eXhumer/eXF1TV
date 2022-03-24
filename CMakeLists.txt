project(
eXF1TV
    LANGUAGES CXX
    VERSION 0.0.0
)

find_package(Qt6 COMPONENTS Core Network WebEngineCore REQUIRED)

qt_wrap_cpp(LIB_MOC
    Include/eXF1TV/F1TV.hxx
)

add_library(
${PROJECT_NAME}
    ${LIB_MOC}
    Source/F1TVService.cxx
)

target_link_libraries(
${PROJECT_NAME}
    PUBLIC
        Qt6::Core
        Qt6::Network
        Qt6::WebEngineCore
)

target_include_directories(
${PROJECT_NAME}
    PUBLIC
        Include/
    PRIVATE
        Include/eXF1TV/
)
