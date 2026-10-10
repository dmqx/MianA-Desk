add_executable(market-rules-test market_rules_test.cpp)
target_link_libraries(market-rules-test PRIVATE MianA.Market)

add_executable(controller-test
    controller_test.cpp
    ../src/MianA.UI/AppController.cpp ../src/MianA.UI/AppController.h
    ../src/MianA.UI/PositionModel.cpp ../src/MianA.UI/PositionModel.h
    ../src/MianA.Infrastructure/AppStore.cpp
    ../src/MianA.Infrastructure/UpdateChecker.h
    ../src/MianA.Market/MarketService.h
    ../src/MianA.Windows/WindowManager.h
)
target_link_libraries(controller-test PRIVATE
    MianA.Core Qt6::Gui Qt6::Network Qt6::Qml
)

foreach(target market-rules-test controller-test)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    endif()
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
        ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1"
        TIMEOUT 30
    )
endforeach()
