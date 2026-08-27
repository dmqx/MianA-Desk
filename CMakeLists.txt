cmake_minimum_required(VERSION 3.21)

include(cmake/MianAVersion.cmake)
project(MianADesk VERSION ${MIANA_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Network Qml Quick QuickControls2 Widgets)
qt_standard_project_setup(REQUIRES 6.8)
qt_policy(SET QTP0004 NEW)

qt_add_executable(MianADesk WIN32
    src/main.cpp
    src/app/AppController.cpp src/app/AppController.h
    src/app/AppStore.cpp src/app/AppStore.h
    src/app/WindowManager.cpp src/app/WindowManager.h
    src/models/PositionModel.cpp src/models/PositionModel.h
    src/services/MarketService.cpp src/services/MarketService.h
    src/services/UpdateChecker.cpp src/services/UpdateChecker.h
    assets/app.rc
)

qt_add_qml_module(MianADesk
    URI MianA
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/PositionEditor.qml
        qml/windows/MainWindow.qml
        qml/windows/CompactWindow.qml
        qml/windows/FloatingWindow.qml
        qml/windows/SettingsWindow.qml
        qml/components/FlatButton.qml
        qml/components/PositionCard.qml
        qml/components/TitleButton.qml
        qml/components/NeutralButton.qml
    RESOURCES assets/miana_desk.ico assets/miana_desk.png
    RESOURCE_PREFIX /qt/qml
)

target_include_directories(MianADesk PRIVATE src)
target_link_libraries(MianADesk PRIVATE
    Qt6::Core Qt6::Gui Qt6::Network Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Widgets
)
if(WIN32)
    target_link_libraries(MianADesk PRIVATE dwmapi user32)
    target_compile_definitions(MianADesk PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
target_compile_definitions(MianADesk PRIVATE
    QT_NO_CAST_FROM_ASCII
    QT_NO_CAST_TO_ASCII
    MIANA_VERSION_STRING="${PROJECT_VERSION}"
)
if(MSVC)
    target_compile_options(MianADesk PRIVATE /W4 /permissive- /utf-8)
endif()

install(TARGETS MianADesk BUNDLE DESTINATION . RUNTIME DESTINATION .)

# Qt 6.8's generated Windows deploy script does not quote an executable path
# containing spaces. Calling windeployqt directly keeps installation reliable
# from directories such as "MianA Desk" and also deploys the scanned QML imports.
if(WIN32)
    install(CODE "
        execute_process(
            COMMAND \"$<TARGET_FILE:Qt6::windeployqt>\"
                    --release
                    --qmldir \"${CMAKE_CURRENT_SOURCE_DIR}/qml\"
                    \"\${CMAKE_INSTALL_PREFIX}/MianADesk.exe\"
            COMMAND_ERROR_IS_FATAL ANY
        )

        # Keep the portable runtime aligned with the legacy package: the app
        # uses the Windows/Basic/Fusion controls and does not need offline
        # redistributable installers, runtime shader compilers, or the other
        # Quick Controls themes.
        file(REMOVE
            \"\${CMAKE_INSTALL_PREFIX}/vc_redist.x64.exe\"
            \"\${CMAKE_INSTALL_PREFIX}/d3dcompiler_47.dll\"
            \"\${CMAKE_INSTALL_PREFIX}/dxcompiler.dll\"
            \"\${CMAKE_INSTALL_PREFIX}/dxil.dll\"
            \"\${CMAKE_INSTALL_PREFIX}/opengl32sw.dll\"
            \"\${CMAKE_INSTALL_PREFIX}/Qt6QuickControls2Imagine.dll\"
            \"\${CMAKE_INSTALL_PREFIX}/Qt6QuickControls2ImagineStyleImpl.dll\"
            \"\${CMAKE_INSTALL_PREFIX}/Qt6QuickControls2Material.dll\"
            \"\${CMAKE_INSTALL_PREFIX}/Qt6QuickControls2MaterialStyleImpl.dll\"
            \"\${CMAKE_INSTALL_PREFIX}/Qt6QuickControls2Universal.dll\"
            \"\${CMAKE_INSTALL_PREFIX}/Qt6QuickControls2UniversalStyleImpl.dll\"
            \"\${CMAKE_INSTALL_PREFIX}/Qt6QuickControls2FluentWinUI3StyleImpl.dll\"
            \"\${CMAKE_INSTALL_PREFIX}/Qt6Svg.dll\"
            \"\${CMAKE_INSTALL_PREFIX}/imageformats/qgif.dll\"
            \"\${CMAKE_INSTALL_PREFIX}/imageformats/qjpeg.dll\"
            \"\${CMAKE_INSTALL_PREFIX}/imageformats/qsvg.dll\"
            \"\${CMAKE_INSTALL_PREFIX}/iconengines/qsvgicon.dll\"
            \"\${CMAKE_INSTALL_PREFIX}/tls/qcertonlybackend.dll\"
            \"\${CMAKE_INSTALL_PREFIX}/qml/QtQuick/Controls/Fusion/qtquickcontrols2fusionstyleplugind.dll\"
            \"\${CMAKE_INSTALL_PREFIX}/qml/QtQuick/Controls/Fusion/impl/qtquickcontrols2fusionstyleimplplugind.dll\"
        )
        file(REMOVE_RECURSE
            \"\${CMAKE_INSTALL_PREFIX}/qml/QtQuick/Controls/Imagine\"
            \"\${CMAKE_INSTALL_PREFIX}/qml/QtQuick/Controls/Material\"
            \"\${CMAKE_INSTALL_PREFIX}/qml/QtQuick/Controls/Universal\"
            \"\${CMAKE_INSTALL_PREFIX}/qml/QtQuick/Controls/FluentWinUI3\"
            \"\${CMAKE_INSTALL_PREFIX}/generic\"
            \"\${CMAKE_INSTALL_PREFIX}/networkinformation\"
            \"\${CMAKE_INSTALL_PREFIX}/qmltooling\"
        )
        file(GLOB qt_translations
            \"\${CMAKE_INSTALL_PREFIX}/translations/*.qm\")
        foreach(qt_translation IN LISTS qt_translations)
            get_filename_component(qt_translation_name
                \"\${qt_translation}\" NAME)
            if(NOT qt_translation_name STREQUAL \"qt_zh_CN.qm\")
                file(REMOVE \"\${qt_translation}\")
            endif()
        endforeach()
        file(GLOB_RECURSE qml_type_descriptions
            \"\${CMAKE_INSTALL_PREFIX}/*.qmltypes\")
        file(REMOVE \${qml_type_descriptions})
    ")
else()
    qt_generate_deploy_qml_app_script(
        TARGET MianADesk OUTPUT_SCRIPT deploy_script NO_UNSUPPORTED_PLATFORM_ERROR
    )
    install(SCRIPT ${deploy_script})
endif()
