# This will build the QMLComponents library

list(APPEND CMAKE_MESSAGE_CONTEXT QMLComponents)

file(GLOB_RECURSE HEADER_FILES "${CMAKE_CURRENT_LIST_DIR}/*.h")
file(GLOB_RECURSE SOURCE_FILES "${CMAKE_CURRENT_LIST_DIR}/*.cpp")

add_library(QMLComponents ${SOURCE_FILES} ${HEADER_FILES})

if(LINUX)
  target_link_libraries(QMLComponents PUBLIC ${_LIB_RT})
  target_link_options(QMLComponents PUBLIC -lrt)
  if(LINUX_LOCAL_BUILD)
    target_compile_definitions(QMLComponents PUBLIC LINUX_LOCAL_BUILD)
  endif()
  if(FLATPAK_USED)
    target_compile_definitions(QMLComponents PUBLIC FLATPAK_USED)
  endif()
endif()

configure_file(${CMAKE_CURRENT_LIST_DIR}/utilities/appdirs.h.in
               ${CMAKE_CURRENT_LIST_DIR}/utilities//appdirs.h)
message(STATUS "appdirs.h is successfully generated...")

target_include_directories(
	QMLComponents
	PUBLIC # JASP
	${PROJECT_SOURCE_DIR}/Common
	${PROJECT_SOURCE_DIR}/Common/jaspColumnEncoder
	${PROJECT_SOURCE_DIR}/CommonData
)

target_link_libraries(
	QMLComponents
	PUBLIC
	Common
	CommonData
	Qt::Core
	Qt::Gui
	Qt::Widgets
	Qt::Qml
	Qt::Quick
	Qt::QuickLayouts
	Qt::QuickControls2
	Qt::QuickControls2Impl
	Qt::QuickWidgets
	Qt::QuickTemplates2
	$<$<NOT:$<BOOL:${FLATPAK_USED}>>:Qt::Core5Compat>
	$<$<BOOL:${FLATPAK_USED}>:/app/lib/$ENV{FLATPAK_ARCH}-linux-gnu/libQt6Core5Compat.so>
)

file(GLOB_RECURSE QML_RESOURCE_FILES "${CMAKE_CURRENT_LIST_DIR}/components/*")

qt_add_resources(
   QMLComponents
   "qmlComponents"
   PREFIX
   "/"
   BASE
   ${CMAKE_CURRENT_LIST_DIR}
   FILES
   ${QML_RESOURCE_FILES})

list(POP_BACK CMAKE_MESSAGE_CONTEXT)
