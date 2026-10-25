# Archipelago multiworld support (tools/oot3d/archipelago). OFF leaves the
# build unchanged: no sources, no definitions, no hooks, no downloads.
option(TRIAEVUM_ENABLE_ARCHIPELAGO
    "Build the Archipelago integration (enabled at runtime with TRIAEVUM_ARCHIPELAGO=1)" OFF)

if(TRIAEVUM_ENABLE_ARCHIPELAGO)
    include(FetchContent)
    # Header-only network dependencies, pinned by commit and hash. The pair
    # asio 1.12.2 / websocketpp 0.8.1 and the apclientpp/wswrap commits are
    # the combination Ship of Harkinian and ap-soeclient build with.
    # Source archives, not git: FetchContent's git path runs `git submodule`.
    FetchContent_Declare(triaevum_ap_asio
        URL https://github.com/chriskohlhoff/asio/archive/refs/tags/asio-1-12-2.tar.gz
        URL_HASH SHA256=1de23266b956674e766cd0b6c929a11259f2284ea8e96b765cc8c67b1689e0fd
        SOURCE_SUBDIR "no CMakeLists.txt")
    FetchContent_Declare(triaevum_ap_websocketpp
        URL https://github.com/zaphoyd/websocketpp/archive/refs/tags/0.8.1.tar.gz
        URL_HASH SHA256=178899de48c02853b55b1ea8681599641cedcdfce59e56beaff3dd0874bf0286
        SOURCE_SUBDIR "no CMakeLists.txt")
    FetchContent_Declare(triaevum_ap_wswrap
        URL https://github.com/black-sliver/wswrap/archive/47438193ec50427ee28aadf294ba57baefd9f3f1.tar.gz
        URL_HASH SHA256=708a8b88b6db3c5581d63ff3c571f9877e392b1947a0ee285817cbd79482a1c5
        SOURCE_SUBDIR "no CMakeLists.txt")
    FetchContent_Declare(triaevum_ap_apclientpp
        URL https://github.com/black-sliver/apclientpp/archive/a5b7b96b6bfbdc7f2831796a4e126bcc09c35da1.tar.gz
        URL_HASH SHA256=f9092fb97cc24f8a208061ce7350857e9322beb110aababb3f8488441fc5461e
        SOURCE_SUBDIR "no CMakeLists.txt")
    FetchContent_MakeAvailable(triaevum_ap_asio triaevum_ap_websocketpp
        triaevum_ap_wswrap triaevum_ap_apclientpp)

    # wss:// needs OpenSSL; it comes from the vcpkg instance TriAevum already
    # bootstraps on Windows.
    if(COMMAND vcpkg_install_packages)
        vcpkg_install_packages(openssl)
    endif()
    find_package(OpenSSL REQUIRED)
    # websocketpp's permessage-deflate extension.
    find_package(ZLIB REQUIRED)

    add_library(triaevum_archipelago STATIC
        tools/oot3d/archipelago/ap_check_observer.cpp
        tools/oot3d/archipelago/ap_cutscene_skip.cpp
        tools/oot3d/archipelago/ap_item_receiver.cpp
        tools/oot3d/archipelago/ap_item_table.cpp
        tools/oot3d/archipelago/ap_location_key.cpp
        tools/oot3d/archipelago/ap_location_table.cpp
        tools/oot3d/archipelago/ap_network_client.cpp
        tools/oot3d/archipelago/ap_session.cpp
        tools/oot3d/archipelago/ap_sha256.cpp
        tools/oot3d/archipelago/ap_sidecar.cpp)
    target_include_directories(triaevum_archipelago PUBLIC tools/oot3d/archipelago)
    target_include_directories(triaevum_archipelago SYSTEM PRIVATE
        "${triaevum_ap_asio_SOURCE_DIR}/asio/include"
        "${triaevum_ap_websocketpp_SOURCE_DIR}"
        "${triaevum_ap_wswrap_SOURCE_DIR}/include"
        "${triaevum_ap_apclientpp_SOURCE_DIR}")
    target_compile_definitions(triaevum_archipelago PRIVATE
        ASIO_STANDALONE
        _WEBSOCKETPP_CPP11_STL_
        AP_NO_SCHEMA)
    # The A32 MemoryBus/GuestState interface; no window or renderer code.
    target_link_libraries(triaevum_archipelago PUBLIC oot3d_native_a32_process)
    target_link_libraries(triaevum_archipelago PRIVATE
        nlohmann_json::nlohmann_json OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB)
    if(WIN32)
        target_compile_definitions(triaevum_archipelago PRIVATE
            _WIN32_WINNT=0x0A00 WIN32_LEAN_AND_MEAN NOMINMAX)
        target_link_libraries(triaevum_archipelago PRIVATE ws2_32 mswsock crypt32)
    endif()
    oot3d_target_defaults(triaevum_archipelago FOLDER Archipelago)

    if(TARGET oot3d_native_game)
        # The F1 tab needs ImGui and the settings window, which only the
        # game target has.
        target_sources(oot3d_native_game PRIVATE tools/oot3d/archipelago/ap_panel.cpp)
        target_link_libraries(oot3d_native_game PRIVATE triaevum_archipelago)
        target_compile_definitions(oot3d_native_game PRIVATE TRIAEVUM_ENABLE_ARCHIPELAGO=1)
    endif()

    add_executable(triaevum_archipelago_tests EXCLUDE_FROM_ALL
        tools/oot3d/archipelago/ap_tests.cpp)
    target_link_libraries(triaevum_archipelago_tests PRIVATE
        triaevum_archipelago nlohmann_json::nlohmann_json)
    target_compile_definitions(triaevum_archipelago_tests PRIVATE
        TRIAEVUM_ARCHIPELAGO_ADDRESSES_JSON="${CMAKE_CURRENT_SOURCE_DIR}/tools/oot3d/archipelago/addresses.json"
        TRIAEVUM_ARCHIPELAGO_LOCATIONS_JSON="${CMAKE_CURRENT_SOURCE_DIR}/runtime/triaevum_resources/archipelago/locations.json"
        TRIAEVUM_ARCHIPELAGO_ITEMS_JSON="${CMAKE_CURRENT_SOURCE_DIR}/runtime/triaevum_resources/archipelago/items.json")
    oot3d_target_defaults(triaevum_archipelago_tests FOLDER Tests)
    add_test(NAME triaevum_archipelago_tests COMMAND triaevum_archipelago_tests)

    # Manual: needs a running Archipelago server (docs/archipelago/PROGRESS.md).
    add_executable(triaevum_archipelago_net_smoke EXCLUDE_FROM_ALL
        tools/oot3d/archipelago/ap_net_smoke.cpp)
    target_link_libraries(triaevum_archipelago_net_smoke PRIVATE triaevum_archipelago)
    oot3d_target_defaults(triaevum_archipelago_net_smoke FOLDER Tests)
endif()
