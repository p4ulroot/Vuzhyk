cmake_minimum_required(VERSION 3.15)

project(Vuzhyk LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Qt5 is required for Windows 7 support (Qt6 не поддерживает Win7)
find_package(Qt5 5.12 REQUIRED COMPONENTS
  Widgets
  Svg
  Network
  WebEngineCore
  WebEngineWidgets
  Quick
  Qml
  Positioning
  PrintSupport
)

# Найти установленный Python (будет использован для бандлинга рантайма)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/src)
set(QSCI_INCLUDE_DIR "C:/QScintilla/src")
set(DLL_DEPEND_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dlldepend")
set(QSCI_LIB "${DLL_DEPEND_DIR}/qscintilla2_qt5.lib")
set(QSCI_DLL "${DLL_DEPEND_DIR}/qscintilla2_qt5.dll")
if (NOT EXISTS "${QSCI_LIB}")
  message(FATAL_ERROR "Missing QScintilla import library: ${QSCI_LIB}")
endif()
if (NOT EXISTS "${QSCI_DLL}")
  message(FATAL_ERROR "Missing QScintilla runtime DLL: ${QSCI_DLL}")
endif()

add_executable(Vuzhyk WIN32
  ${SRC_DIR}/main.cpp
  ${SRC_DIR}/MainWindow.cpp
  ${SRC_DIR}/MainWindow.h
  ${SRC_DIR}/CodeEditor.cpp
  ${SRC_DIR}/CodeEditor.h
  ${SRC_DIR}/TitleBar.cpp
  ${SRC_DIR}/TitleBar.h
  ${SRC_DIR}/AnimatedMenu.cpp
  ${SRC_DIR}/AnimatedMenu.h
  ${SRC_DIR}/ConsoleWidget.cpp
  ${SRC_DIR}/ConsoleWidget.h
  ${SRC_DIR}/WindowFrameOverlay.cpp
  ${SRC_DIR}/WindowFrameOverlay.h
  ${SRC_DIR}/SettingsWidget.cpp
  ${SRC_DIR}/SettingsWidget.h
  ${SRC_DIR}/PythonInterpreterPaths.cpp
  ${SRC_DIR}/PythonInterpreterPaths.h
  ${SRC_DIR}/extensions/ExtensionManager.cpp
  ${SRC_DIR}/HelpWidget.cpp
  ${SRC_DIR}/HelpWidget.h
  ${SRC_DIR}/pyrobeditor/PyrobEditorWidget.cpp
  ${SRC_DIR}/pyrobeditor/PyrobEditorWidget.h
  ${SRC_DIR}/pyrobeditor/grideditor.cpp
  ${SRC_DIR}/pyrobeditor/grideditor.h
  ${SRC_DIR}/pyrobeditor/projectmodel.cpp
  ${SRC_DIR}/pyrobeditor/projectmodel.h
  ${SRC_DIR}/sync/LibrarySyncCrypto.cpp
  ${SRC_DIR}/sync/LibrarySyncCrypto.h
  ${SRC_DIR}/sync/LibrarySyncManager.cpp
  ${SRC_DIR}/sync/LibrarySyncManager.h
  ${SRC_DIR}/sync/LibrarySyncServerControl.cpp
  ${SRC_DIR}/sync/LibrarySyncServerControl.h
  ${SRC_DIR}/agent/AgentPrompts.cpp
  ${SRC_DIR}/agent/AgentPrompts.h
  ${SRC_DIR}/agent/AgentEdit.cpp
  ${SRC_DIR}/agent/AgentEdit.h
  ${SRC_DIR}/agent/AgentHttps.cpp
  ${SRC_DIR}/agent/AgentHttps.h
  ${SRC_DIR}/agent/AgentOpenRouterKeys.cpp
  ${SRC_DIR}/agent/AgentOpenRouterKeys.h
  ${SRC_DIR}/agent/LmStudioClient.cpp
  ${SRC_DIR}/agent/LmStudioClient.h
  ${SRC_DIR}/agent/AgentPanel.cpp
  ${SRC_DIR}/agent/AgentPanel.h
  ${SRC_DIR}/sea/SnakeGame.cpp
  ${SRC_DIR}/sea/SnakeGame.h
  ${CMAKE_CURRENT_LIST_DIR}/resources.qrc
)

# Встраиваем иконку EXE через ресурс .rc (Windows)
if (WIN32)
  target_sources(Vuzhyk PRIVATE ${CMAKE_CURRENT_LIST_DIR}/app.rc)
  set_target_properties(Vuzhyk PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

target_include_directories(Vuzhyk PRIVATE ${QSCI_INCLUDE_DIR})

add_library(QScintilla SHARED IMPORTED GLOBAL)
set_target_properties(QScintilla PROPERTIES
  IMPORTED_IMPLIB "${QSCI_LIB}"
  IMPORTED_LOCATION "${QSCI_DLL}"
)

target_link_libraries(Vuzhyk PRIVATE Qt5::Widgets Qt5::Svg Qt5::Network QScintilla)
if (WIN32)
  target_link_libraries(Vuzhyk PRIVATE winhttp)
endif()

# Embedded browser extension
set(EMBEDDED_BROWSER_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/embedded-browser/src)
set(EMBEDDED_BROWSER_SOURCES
  ${EMBEDDED_BROWSER_SRC_DIR}/main.cpp
  ${EMBEDDED_BROWSER_SRC_DIR}/BrowserWindow.cpp
  ${EMBEDDED_BROWSER_SRC_DIR}/BrowserWindow.h
)
set(EMBEDDED_BROWSER_OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/extensions/EmbeddedBrowserExtension)
file(MAKE_DIRECTORY ${EMBEDDED_BROWSER_OUTPUT_DIR})

add_executable(EmbeddedBrowserExtension WIN32 ${EMBEDDED_BROWSER_SOURCES})
target_link_libraries(EmbeddedBrowserExtension PRIVATE
  Qt5::Widgets
  Qt5::Network
  Qt5::WebEngineCore
  Qt5::WebEngineWidgets
  Qt5::Quick
  Qt5::Qml
  Qt5::WebChannel
  Qt5::Positioning
  Qt5::PrintSupport
)
target_include_directories(EmbeddedBrowserExtension PRIVATE ${EMBEDDED_BROWSER_SRC_DIR})
set_target_properties(EmbeddedBrowserExtension PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${EMBEDDED_BROWSER_OUTPUT_DIR}
  RUNTIME_OUTPUT_DIRECTORY_DEBUG ${EMBEDDED_BROWSER_OUTPUT_DIR}
  RUNTIME_OUTPUT_DIRECTORY_RELEASE ${EMBEDDED_BROWSER_OUTPUT_DIR}
)

add_dependencies(Vuzhyk EmbeddedBrowserExtension)

get_filename_component(_qt5_lib_dir "${Qt5Core_DIR}" DIRECTORY)
get_filename_component(_qt5_lib_dir "${_qt5_lib_dir}" DIRECTORY)
get_filename_component(Qt5_BASE_DIR "${_qt5_lib_dir}" DIRECTORY)
set(Qt5_BIN_DIR "${Qt5_BASE_DIR}/bin")

# Проверяем, существует ли директория bin
if(NOT EXISTS "${Qt5_BIN_DIR}")
  # Пробуем альтернативный способ - через переменную окружения
  if(DEFINED ENV{QTDIR})
    set(Qt5_BIN_DIR "$ENV{QTDIR}/bin")
  endif()
  if(NOT EXISTS "${Qt5_BIN_DIR}")
    message(WARNING "Qt5_BIN_DIR not found: ${Qt5_BIN_DIR}")
  endif()
endif()

# Копируем только Release DLL (без суффикса 'd')
set(Qt5_DLLS
  Qt5Core.dll
  Qt5Gui.dll
  Qt5Widgets.dll
  Qt5WebEngineCore.dll
  Qt5WebEngineWidgets.dll
  Qt5Quick.dll
  Qt5Qml.dll
  Qt5WebChannel.dll
  Qt5Network.dll
  Qt5Positioning.dll
  Qt5PrintSupport.dll
)

foreach(_dll IN ITEMS Qt5WebChannel.dll)
  file(COPY "${Qt5_BIN_DIR}/${_dll}"
       DESTINATION "${EMBEDDED_BROWSER_OUTPUT_DIR}")
endforeach()

# Отладочное сообщение для проверки пути
message(STATUS "Qt5_BIN_DIR: ${Qt5_BIN_DIR}")
message(STATUS "EmbeddedBrowserExtension output dir: ${EMBEDDED_BROWSER_OUTPUT_DIR}")

set(EMBEDDED_BROWSER_COPY_COMMANDS
  COMMAND ${CMAKE_COMMAND} -E make_directory ${EMBEDDED_BROWSER_OUTPUT_DIR}
)
foreach(_dll IN LISTS Qt5_DLLS)
  set(_dll_src "${Qt5_BIN_DIR}/${_dll}")
  set(_dll_dst "${EMBEDDED_BROWSER_OUTPUT_DIR}/${_dll}")
  list(APPEND EMBEDDED_BROWSER_COPY_COMMANDS
    COMMAND ${CMAKE_COMMAND} -E copy "${_dll_src}" "${_dll_dst}"
  )
endforeach()

if(EXISTS "${Qt5_BASE_DIR}/plugins/platforms")
  list(APPEND EMBEDDED_BROWSER_COPY_COMMANDS
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${Qt5_BASE_DIR}/plugins/platforms"
            "${EMBEDDED_BROWSER_OUTPUT_DIR}/platforms"
  )
endif()

add_custom_command(TARGET EmbeddedBrowserExtension POST_BUILD
  ${EMBEDDED_BROWSER_COPY_COMMANDS}
  COMMENT "Copying Qt DLLs to EmbeddedBrowserExtension"
  VERBATIM
)

if(EXISTS "${Qt5_BASE_DIR}/resources")
  add_custom_command(TARGET EmbeddedBrowserExtension POST_BUILD
    COMMAND ${CMAKE_COMMAND}
            -DQT_SOURCE_DIR=${Qt5_BASE_DIR}/resources
            -DQT_DEST_DIR=${EMBEDDED_BROWSER_OUTPUT_DIR}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CopyQtWebEngineContents.cmake"
    COMMENT "Flatten Qt WebEngine resources into extension root"
    VERBATIM
  )
else()
  message(WARNING "Qt WebEngine resources directory not found: ${Qt5_BASE_DIR}/resources")
endif()

if(EXISTS "${Qt5_BASE_DIR}/translations/qtwebengine_locales")
  add_custom_command(TARGET EmbeddedBrowserExtension POST_BUILD
    COMMAND ${CMAKE_COMMAND}
            -DQT_SOURCE_DIR=${Qt5_BASE_DIR}/translations/qtwebengine_locales
            -DQT_DEST_DIR=${EMBEDDED_BROWSER_OUTPUT_DIR}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CopyQtWebEngineContents.cmake"
    COMMENT "Flatten Qt WebEngine locales into extension root"
    VERBATIM
  )
else()
  message(WARNING "Qt WebEngine locales directory not found: ${Qt5_BASE_DIR}/translations/qtwebengine_locales")
endif()

if(EXISTS "${Qt5_BIN_DIR}/windeployqt.exe")
  add_custom_command(TARGET EmbeddedBrowserExtension POST_BUILD
    COMMAND "${Qt5_BIN_DIR}/windeployqt.exe"
            --qmldir "${EMBEDDED_BROWSER_SRC_DIR}"
            --force
            --release
            "$<TARGET_FILE:EmbeddedBrowserExtension>"
    COMMENT "Running windeployqt to gather all Qt dependencies"
    VERBATIM
  )
else()
  message(WARNING "windeployqt.exe not found in ${Qt5_BIN_DIR}, skipping automatic deployment")
endif()

add_custom_command(TARGET Vuzhyk POST_BUILD
  COMMAND cmd /c "\"${CMAKE_CURRENT_SOURCE_DIR}/scripts/copy-openssl.bat\" \"$<TARGET_FILE_DIR:Vuzhyk>\" \"${Qt5_BASE_DIR}\" \"${CMAKE_CURRENT_SOURCE_DIR}\""
  COMMENT "Copying OpenSSL DLLs for HTTPS (OpenRouter)"
)

add_custom_command(TARGET Vuzhyk POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Vuzhyk>/extensions"
  COMMAND ${CMAKE_COMMAND} -E remove_directory "$<TARGET_FILE_DIR:Vuzhyk>/extensions/EmbeddedBrowserExtension"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${EMBEDDED_BROWSER_OUTPUT_DIR}
          "$<TARGET_FILE_DIR:Vuzhyk>/extensions/EmbeddedBrowserExtension"
  COMMENT "Deploying embedded browser extension alongside Vuzhyk"
)

add_custom_command(TARGET Vuzhyk POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${QSCI_DLL}"
          "$<TARGET_FILE_DIR:Vuzhyk>/qscintilla2_qt5.dll"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${QSCI_LIB}"
          "$<TARGET_FILE_DIR:Vuzhyk>/qscintilla2_qt5.lib"
  COMMENT "Deploying QScintilla runtime files"
)

set(SUMATRA_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tools/pdf")
if(EXISTS "${SUMATRA_SOURCE_DIR}")
  add_custom_command(TARGET Vuzhyk POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${SUMATRA_SOURCE_DIR}"
            "$<TARGET_FILE_DIR:Vuzhyk>/tools/pdf"
    COMMENT "Copying SumatraPDF folder into installation directory"
  )
else()
  message(WARNING "SumatraPDF folder not found: ${SUMATRA_SOURCE_DIR}")
endif()


add_custom_command(TARGET EmbeddedBrowserExtension POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${DLL_DEPEND_DIR}/Qt5WebChannel.dll"
          "${EMBEDDED_BROWSER_OUTPUT_DIR}/Qt5WebChannel.dll"
  COMMENT "Copying Qt5WebChannel.dll from dlldepend to extension"
  VERBATIM
)
add_custom_command(TARGET Vuzhyk POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${DLL_DEPEND_DIR}/Qt5WebChannel.dll"
          "$<TARGET_FILE_DIR:Vuzhyk>/extensions/EmbeddedBrowserExtension/Qt5WebChannel.dll"
  COMMENT "Copying Qt5WebChannel.dll from dlldepend to deployment"
)

# FilePicker - отдельный exe для выбора файлов (использует нативный Windows API)
add_executable(FilePicker WIN32
  ${SRC_DIR}/filepicker.cpp
)

# Для Windows используем COM библиотеки
if (WIN32)
  target_link_libraries(FilePicker PRIVATE ole32 shell32 comdlg32)
endif()

# Куда собирать встроенный Python
set(VUZHYK_PY_RUNTIME_DIR "${CMAKE_BINARY_DIR}/runtime/python")
set(VUZHYK_LOCAL_PYTHON_DIR "${CMAKE_CURRENT_LIST_DIR}/Python38")

# Цель для копирования Python из системы внутрь билда
add_custom_target(bundle_python ALL
  COMMAND ${CMAKE_COMMAND}
          -DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}
          -DVUZHYK_DEST_DIR=${VUZHYK_PY_RUNTIME_DIR}
          -DVUZHYK_LOCAL_PYTHON_DIR=${VUZHYK_LOCAL_PYTHON_DIR}
          -P ${CMAKE_CURRENT_LIST_DIR}/cmake/BundlePython.cmake
  COMMENT "Bundling Python runtime into ${VUZHYK_PY_RUNTIME_DIR}"
  VERBATIM
)

if(EXISTS "${VUZHYK_LOCAL_PYTHON_DIR}/Doc")
  add_custom_command(TARGET bundle_python POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${VUZHYK_LOCAL_PYTHON_DIR}/Doc"
            "${VUZHYK_PY_RUNTIME_DIR}/Doc"
  )
endif()

add_dependencies(Vuzhyk bundle_python)

# Установка: бинарь + папка с Python
install(TARGETS Vuzhyk FilePicker RUNTIME DESTINATION .)
install(DIRECTORY "${VUZHYK_PY_RUNTIME_DIR}/" DESTINATION "Python38")


