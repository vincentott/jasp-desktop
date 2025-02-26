# This will build the CommonData library
#
# On Linux,
#   - We need to link to the `librt.so`, and for some reason, CMake cannot find it
#     easily, so, Libraries module tries to find it and provides it to CommonData here.
#   - Since we are not using Conan, I link to the libjsoncpp, using the PkgConfig variables
#
#
list(APPEND CMAKE_MESSAGE_CONTEXT CommonData)

make_includable(internalDbDefinition.sql internalDbDefinition.h)

file(GLOB_RECURSE HEADER_FILES	"${CMAKE_CURRENT_LIST_DIR}/*.h")
file(GLOB_RECURSE SOURCE_FILES	"${CMAKE_CURRENT_LIST_DIR}/*.cpp")
file(GLOB_RECURSE SQL_FILES		"${CMAKE_CURRENT_LIST_DIR}/*.sql")

add_library(CommonData ${SOURCE_FILES} ${HEADER_FILES} ${SQL_FILES})


if(LINUX)

  target_link_libraries(CommonData PUBLIC ${_LIB_RT})
  target_link_options(CommonData PUBLIC -lrt)

endif()

target_include_directories(
	CommonData
	PUBLIC # JASP
	${PROJECT_SOURCE_DIR}/Common
	${PROJECT_SOURCE_DIR}/Common/jaspColumnEncoder
	# R
	${R_INCLUDE_PATH}
	${R_HOME_PATH}/include
	${RCPP_PATH}/include
	)

target_link_libraries(
	CommonData
	PUBLIC
	Common
	LibArchive::LibArchive
	SQLite::SQLite3
	#
	$<$<BOOL:${JASP_USES_QT_HERE}>:Qt::Core>)

target_compile_definitions(
  CommonData PUBLIC $<$<BOOL:${JASP_USES_QT_HERE}>:JASP_USES_QT_HERE>
                $<$<BOOL:${JASP_TIMER_USED}>:PROFILE_JASP>
				JSONCPP_NO_LOCALE_SUPPORT	)

if(APPLE)
  target_compile_definitions(CommonData PUBLIC BOOST_INTERPROCESS_SHARED_DIR_FUNC)
elseif(WINDOWS)

  target_compile_definitions(
    CommonData
    PUBLIC BOOST_INTERPROCESS_BOOTSTAMP_IS_SESSION_MANAGER_BASED
           BOOST_WINDOWS
           NOMINMAX
           WIN32_LEAN_AND_MEAN)
endif()

if(IWYU_EXECUTABLE AND RUN_IWYU)
  set_target_properties(CommonData PROPERTIES CXX_INCLUDE_WHAT_YOU_USE
                                          ${IWYU_EXECUTABLE})
endif()

list(POP_BACK CMAKE_MESSAGE_CONTEXT)
