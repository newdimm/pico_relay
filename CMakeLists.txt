cmake_minimum_required(VERSION 3.13)

set(HAVE_WIRELESS 1)
set(HAVE_MQTT 1)
set(HAVE_TELEGRAM 1)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)



if (HAVE_WIRELESS)
    set(PICO_BOARD pico2_w CACHE STRING "Board type")
    add_compile_options(-DHAVE_WIRELESS)

    if (DEFINED ENV{WIFI_SSID} AND (NOT WIFI_SSID))
        set(WIFI_SSID $ENV{WIFI_SSID})
        message("Using WIFI_SSID from environment ('${WIFI_SSID}')")
    endif()

    if (DEFINED ENV{WIFI_PASSWORD} AND (NOT WIFI_PASSWORD))
        set(WIFI_PASSWORD $ENV{WIFI_PASSWORD})
        message("Using WIFI_PASSWORD from environment")
    endif()

    if (NOT WIFI_PASSWORD OR NOT WIFI_SSID)
        message("WIFI_PASSWORD and WIFI_SSID must be defined")
        return()
    endif()
     
    set(WIFI_SSID "${WIFI_SSID}" CACHE INTERNAL "WiFi SSID for examples")
    set(WIFI_PASSWORD "${WIFI_PASSWORD}" CACHE INTERNAL "WiFi password for examples")
else()
    set(PICO_BOARD pico CACHE STRING "Board type")
endif()

if (HAVE_TELEGRAM)

    add_compile_options(-DHAVE_TELEGRAM)

    if (DEFINED ENV{TELEGRAM_TOKEN} AND (NOT TELEGRAM_TOKEN))
        set(TELEGRAM_TOKEN $ENV{TELEGRAM_TOKEN})
        message("Using TELEGRAM_TOKEN from environment ('${TELEGRAM_TOKEN}')")
    endif()

    if (NOT TELEGRAM_TOKEN)
        message("TELEGRAM_TOKEN must be defined")
        return()
    endif()
endif()

if (HAVE_MQTT)
    add_compile_options(-DHAVE_MQTT)

    if (DEFINED ENV{MQTT_SERVER} AND (NOT MQTT_SERVER))
        set(MQTT_SERVER $ENV{MQTT_SERVER})
        message("Using MQTT_SERVER from environment ('${MQTT_SERVER}')")
    endif()
    if (NOT MQTT_SERVER)
        message("Skipping MQTT example as MQTT_SERVER is not defined")
        return()
    endif()
    # Define the name of an MQTT broker/server to enable this example
    set(MQTT_SERVER "${MQTT_SERVER}" CACHE INTERNAL "MQTT server for examples")

    if (DEFINED ENV{MQTT_USERNAME} AND (NOT MQTT_USERNAME))
        set(MQTT_USERNAME $ENV{MQTT_USERNAME})
        message("Using MQTT_USERNAME from environment ('${MQTT_USERNAME}')")
    endif()
    set(MQTT_USERNAME "${MQTT_USERNAME}" CACHE INTERNAL "MQTT user name for examples")
    if (DEFINED ENV{MQTT_PASSWORD} AND (NOT MQTT_PASSWORD))
        set(MQTT_PASSWORD $ENV{MQTT_PASSWORD})
        message("Using MQTT_PASSWORD from environment")
    endif()
    set(MQTT_PASSWORD "${MQTT_PASSWORD}" CACHE INTERNAL "MQTT password for examples")

    # Set path to the certificate include file
    if (NOT MQTT_CERT_PATH)
        set(MQTT_CERT_PATH ${CMAKE_CURRENT_LIST_DIR}/certs/${MQTT_SERVER})
    endif()

    # Set the name of the certificate include file
    if (NOT MQTT_CERT_INC)
        set(MQTT_CERT_INC mqtt_client.inc)
    endif()

endif()

include(pico_sdk_import.cmake)
project(relay C CXX ASM)

pico_sdk_init()

add_executable(relay)

target_sources(relay PRIVATE
    relay.c
    network.h
    mqtt.h
    telegram.h
)

target_include_directories(relay PRIVATE ${CMAKE_CURRENT_LIST_DIR} )

target_link_libraries(relay pico_stdlib)
target_link_libraries(relay pico_time)
target_link_libraries(relay hardware_adc)

if (HAVE_WIRELESS)

    target_compile_definitions(relay PRIVATE
        WIFI_SSID=\"${WIFI_SSID}\"
        WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
        )

    target_sources(relay PRIVATE
        network.c
    )

    target_link_libraries(relay pico_cyw43_arch_lwip_threadsafe_background)
endif()

if (HAVE_TELEGRAM)

    target_compile_definitions(relay PRIVATE
        TELEGRAM_TOKEN=\"${TELEGRAM_TOKEN}\"
    )

    target_link_libraries(relay 
        pico_lwip_http
        pico_lwip_mbedtls
        pico_mbedtls
    )

    target_sources(relay PRIVATE
        telegram.c
        json.c
        json.h
    )

    # Ignore warnings from lwip code
    set_source_files_properties(
        ${PICO_LWIP_PATH}/src/apps/altcp_tls/altcp_tls_mbedtls.c
        PROPERTIES
        COMPILE_OPTIONS "-Wno-unused-result"
        )
endif()

if (HAVE_MQTT)
    target_sources(relay PRIVATE
        mqtt_client.c
    )
    target_link_libraries(relay
        pico_lwip_mqtt
        pico_mbedtls
        pico_lwip_mbedtls
    )

    target_compile_definitions(relay PRIVATE
        MQTT_SERVER=\"${MQTT_SERVER}\"
        )
    if (EXISTS "${MQTT_CERT_PATH}/${MQTT_CERT_INC}")
        target_compile_definitions(relay PRIVATE
            MQTT_CERT_INC=\"${MQTT_CERT_INC}\" # contains the tls certificates for MQTT_SERVER needed by the client
            ALTCP_MBEDTLS_AUTHMODE=MBEDTLS_SSL_VERIFY_REQUIRED
            )
        target_include_directories(relay PRIVATE
            ${MQTT_CERT_PATH}
            )
    endif()
    if (MQTT_USERNAME AND MQTT_PASSWORD)
        target_compile_definitions(relay PRIVATE
            MQTT_USERNAME=\"${MQTT_USERNAME}\"
            MQTT_PASSWORD=\"${MQTT_PASSWORD}\"
        )
    endif()

    # Ignore warnings from lwip code
    set_source_files_properties(
        ${PICO_LWIP_PATH}/src/apps/altcp_tls/altcp_tls_mbedtls.c
        PROPERTIES
        COMPILE_OPTIONS "-Wno-unused-result"
        )
endif()

pico_enable_stdio_uart(relay 1)

pico_add_extra_outputs(relay)

