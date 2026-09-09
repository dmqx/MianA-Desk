cmake_minimum_required(VERSION 3.22)

include(cmake/MianAVersion.cmake)
project(MianADesk VERSION ${MIANA_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.11.2 REQUIRED COMPONENTS
    Core Gui Network Qml Quick QuickControls2 Widgets
)
qt_standard_project_setup(REQUIRES 6.11.2)
qt_policy(SET QTP0004 NEW)

add_library(MianA.Core STATIC
    src/MianA.Core/AppSettings.h
    src/MianA.Core/IntradaySeries.cpp src/MianA.Core/IntradaySeries.h
    src/MianA.Core/MarketRules.cpp src/MianA.Core/MarketRules.h
    src/MianA.Core/Position.h
    src/MianA.Core/Quote.h
)
target_include_directories(MianA.Core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(MianA.Core PUBLIC Qt6::Core)

add_library(MianA.Market STATIC
    src/MianA.Market/MarketService.cpp src/MianA.Market/MarketService.h
    src/MianA.Market/QuoteProviders.cpp src/MianA.Market/QuoteProviders.h
)
target_include_directories(MianA.Market PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(MianA.Market PUBLIC MianA.Core Qt6::Core Qt6::Network)

add_library(MianA.Infrastructure STATIC
    src/MianA.Infrastructure/AppStore.cpp src/MianA.Infrastructure/AppStore.h
    src/MianA.Infrastructure/UpdateChecker.cpp src/MianA.Infrastructure/UpdateChecker.h
)
target_include_directories(MianA.Infrastructure PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(MianA.Infrastructure PUBLIC MianA.Core Qt6::Core Qt6::Network)
target_compile_definitions(MianA.Infrastructure PRIVATE
    MIANA_VERSION_STRING="${PROJECT_VERSION}"
)

add_library(MianA.Windows STATIC
    src/MianA.Windows/WindowManager.cpp src/MianA.Windows/WindowManager.h
)
target_include_directories(MianA.Windows PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(MianA.Windows PUBLIC
    Qt6::Core Qt6::Gui Qt6::Network Qt6::Widgets
)
if(WIN32)
    target_link_libraries(MianA.Windows PRIVATE dwmapi user32 ole32)
    target_compile_definitions(MianA.Windows PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

qt_add_executable(MianADesk WIN32
    src/main.cpp
    src/MianA.UI/AppController.cpp src/MianA.UI/AppController.h
    src/MianA.UI/PositionModel.cpp src/MianA.UI/PositionModel.h
    assets/app.rc
)
target_include_directories(MianADesk PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MianA.UI
)
target_link_libraries(MianADesk PRIVATE
    MianA.Core MianA.Market MianA.Infrastructure MianA.Windows
    Qt6::Core Qt6::Gui Qt6::Network Qt6::Qml Qt6::Quick
    Qt6::QuickControls2 Qt6::Widgets
)

qt_add_qml_module(MianADesk
    URI MianA
    VERSION 1.0
    QML_FILES
        src/MianA.UI/qml/Main.qml
        src/MianA.UI/qml/Views/MainWindow.qml
        src/MianA.UI/qml/Views/CompactWindow.qml
        src/MianA.UI/qml/Views/FloatingWindow.qml
        src/MianA.UI/qml/Views/SettingsWindow.qml
        src/MianA.UI/qml/Views/PositionEditor.qml
        src/MianA.UI/qml/Controls/AppButton.qml
        src/MianA.UI/qml/Controls/PositionCard.qml
        src/MianA.UI/qml/Controls/CompactCard.qml
        src/MianA.UI/qml/Controls/Sparkline.qml
    RESOURCES assets/miana_desk.ico assets/miana_desk.png
    RESOURCE_PREFIX /qt/qml
)

foreach(target MianA.Core MianA.Market MianA.Infrastructure MianA.Windows MianADesk)
    target_compile_features(${target} PUBLIC cxx_std_23)
    target_compile_definitions(${target} PRIVATE
        QT_NO_CAST_FROM_ASCII
        QT_NO_CAST_TO_ASCII
    )
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    endif()
endforeach()
target_compile_definitions(MianADesk PRIVATE
    MIANA_VERSION_STRING="${PROJECT_VERSION}"
)
install(TARGETS MianADesk BUNDLE DESTINATION . RUNTIME DESTINATION .)

# Qt's generated Windows deploy script does not quote an executable path
# containing spaces. Calling windeployqt directly keeps installation reliable
# from directories such as "MianA Desk" and also deploys the scanned QML imports.
if(WIN32)
    install(CODE "
        execute_process(
            COMMAND \"$<TARGET_FILE:Qt6::windeployqt>\"
                    --release
                    --qmldir \"${CMAKE_CURRENT_SOURCE_DIR}/src/MianA.UI/qml\"
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
