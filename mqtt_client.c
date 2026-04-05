/**
 * Copyright (c) 2022 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

//
// Created by elliot on 25/05/24.
//
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/unique_id.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/adc.h"
#include "lwip/apps/mqtt.h"
#include "lwip/apps/mqtt_priv.h" // needed to set hostname
#include "lwip/dns.h"
#include "lwip/altcp_tls.h"

#include "network.h"

// Temperature
#ifndef TEMPERATURE_UNITS
#define TEMPERATURE_UNITS 'C' // Set to 'F' for Fahrenheit
#endif

#ifndef MQTT_SERVER
#error Need to define MQTT_SERVER
#endif

// This file includes your client certificate for client server authentication
#ifdef MQTT_CERT_INC
#include MQTT_CERT_INC
#endif

#ifndef MQTT_TOPIC_LEN
#define MQTT_TOPIC_LEN 100
#endif

#define MQTT_DNS_RETRY_TIMEOUT_MS 5000


typedef struct {
    bool init_done;
    bool client_starting;
    bool dns_resolved;
    bool dns_requested;

    absolute_time_t dns_failed_time;

    mqtt_client_t* mqtt_client_inst;
    struct mqtt_connect_client_info_t mqtt_client_info;
    char data[MQTT_OUTPUT_RINGBUF_SIZE];
    char topic[MQTT_TOPIC_LEN];
    uint32_t len;
    ip_addr_t mqtt_server_address;
    bool connect_done;
    int subscribe_count;
    bool stop_client;
} MQTT_CLIENT_DATA_T;

// keep alive in seconds
#define MQTT_KEEP_ALIVE_S 60

// qos passed to mqtt_subscribe
// At most once (QoS 0)
// At least once (QoS 1)
// Exactly once (QoS 2)
#define MQTT_PUBLISH_QOS 1
#define MQTT_PUBLISH_RETAIN 1

#ifndef MQTT_DEVICE_NAME
#define MQTT_DEVICE_NAME "pico"
#endif

MQTT_CLIENT_DATA_T rmqtt;

static void pub_request_cb(__unused void *arg, err_t err)
{
    if (err != 0) {
        
        printf("mqtt: pub_request_cb failed %d", err);
    }
}

void mqtt_send(const char *topic, const char *message)
{
    if (!rmqtt.connect_done)
    {
        return;
    }

    printf("mqtt: publish <%s> <%s>\n", topic, message);
    mqtt_publish(rmqtt.mqtt_client_inst, topic, message, strlen(message), 
                 MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, &rmqtt);
}

static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
    MQTT_CLIENT_DATA_T* state = (MQTT_CLIENT_DATA_T*)arg;
    state->client_starting = false;

    if (status == MQTT_CONNECT_ACCEPTED) {
        state->connect_done = true;
        //sub_unsub_topics(state, true); // subscribe;

        // indicate online
        if (state->mqtt_client_info.will_topic) {
            mqtt_send(state->mqtt_client_info.will_topic, "started");
        }

    } else if (status == MQTT_CONNECT_DISCONNECTED) {
        if (!state->connect_done) {
           printf("mqtt: failed to connect to mqtt server\n");
        }
        else {
            printf("mqtt: disconnected\n");
        }
        state->connect_done = false;
    }
    else {
        printf("mqtt: unexpected status %d\n", status);
        state->connect_done = false;
    }
}

static void start_client(MQTT_CLIENT_DATA_T *state) {
#if LWIP_ALTCP && LWIP_ALTCP_TLS
    const int port = MQTT_TLS_PORT;
    printf("mqtt: using TLS\n");
#else
    const int port = MQTT_PORT;
    printf("mqtt: warning: not using TLS\n");
#endif

    state->mqtt_client_inst = mqtt_client_new();
    if (!state->mqtt_client_inst) {
        printf("mqtt: client instance creation error\n");
        return;
    }
    printf("mqtt: IP address of this device %s\n", ipaddr_ntoa(&(netif_list->ip_addr)));
    printf("mqtt: connecting to mqtt server at %s\n", ipaddr_ntoa(&state->mqtt_server_address));

    state->client_starting = true;

    cyw43_arch_lwip_begin();
    if (mqtt_client_connect(state->mqtt_client_inst, &state->mqtt_server_address, port, 
                            mqtt_connection_cb, 
                            state, &state->mqtt_client_info) != ERR_OK) {
        printf("mqtt: broker connection error\n");
        mqtt_client_free(state->mqtt_client_inst);
        state->mqtt_client_inst = NULL;
        state->client_starting = false;
        return;
    }
#if LWIP_ALTCP && LWIP_ALTCP_TLS
    // This is important for MBEDTLS_SSL_SERVER_NAME_INDICATION
    mbedtls_ssl_set_hostname(altcp_tls_context(state->mqtt_client_inst->conn), MQTT_SERVER);
#endif
    //mqtt_set_inpub_callback(state->mqtt_client_inst, mqtt_incoming_publish_cb, mqtt_incoming_data_cb, state);
    cyw43_arch_lwip_end();
}

void mqtt_init(void) {
    printf("mqtt: init\n");

    rmqtt.mqtt_client_info.client_id = "pico_relay";
    rmqtt.mqtt_client_info.keep_alive = MQTT_KEEP_ALIVE_S; // Keep alive in sec
#if defined(MQTT_USERNAME) && defined(MQTT_PASSWORD)
    rmqtt.mqtt_client_info.client_user = MQTT_USERNAME;
    rmqtt.mqtt_client_info.client_pass = MQTT_PASSWORD;
#else
    rmqtt.mqtt_client_info.client_user = NULL;
    rmqtt.mqtt_client_info.client_pass = NULL;
#endif
    rmqtt.mqtt_client_info.will_topic = "/log";
    rmqtt.mqtt_client_info.will_msg = "connected";
    rmqtt.mqtt_client_info.will_qos = 1;
    rmqtt.mqtt_client_info.will_retain = true;
#if LWIP_ALTCP && LWIP_ALTCP_TLS
    // TLS enabled
#ifdef MQTT_CERT_INC
    static const uint8_t ca_cert[] = TLS_ROOT_CERT;
    static const uint8_t client_key[] = TLS_CLIENT_KEY;
    static const uint8_t client_cert[] = TLS_CLIENT_CERT;
    // This confirms the indentity of the server and the client
    rmqtt.mqtt_client_info.tls_config = altcp_tls_create_config_client_2wayauth(ca_cert, sizeof(ca_cert),
            client_key, sizeof(client_key), NULL, 0, client_cert, sizeof(client_cert));
#if ALTCP_MBEDTLS_AUTHMODE != MBEDTLS_SSL_VERIFY_REQUIRED
    printf("mqtt: warning: tls without verification is insecure\n");
#endif
#else
    rmqtt.client_info.tls_config = altcp_tls_create_config_client(NULL, 0);
    printf("mqtt: warning: tls without a certificate is insecure\n");
#endif
#endif
    rmqtt.init_done = true;
}


// Call back with a DNS result
static void dns_found(const char *hostname, const ip_addr_t *ipaddr, void *arg) {
    MQTT_CLIENT_DATA_T *state = (MQTT_CLIENT_DATA_T*)arg;
    if (ipaddr) {
        state->mqtt_server_address = *ipaddr;
        printf("mqtt: dns <%s> resolved to %s\n", hostname, ipaddr_ntoa(&state->mqtt_server_address));
        state->dns_resolved = true;
    } else {
        printf("mqtt: dns <%s> resolve failed\n", hostname);
        rmqtt.dns_failed_time = get_absolute_time();
    }
    state->dns_requested = false;
}

void mqtt_periodic(void)
{
    if (!rmqtt.init_done) {
        return;
    }

    if (!rnet.connected) {
        return;
    }

    if (!rnet.ready) {
        return;
    }

    if (!rmqtt.connect_done) {

        if (!rmqtt.dns_resolved) {
            if (!rmqtt.dns_requested) {
                if (!rmqtt.dns_failed_time ||
                        absolute_time_diff_us(rmqtt.dns_failed_time, get_absolute_time())
                           > MQTT_DNS_RETRY_TIMEOUT_MS * 1000)
                {
                    rmqtt.dns_failed_time = 0;

                    // We are not in a callback so locking is needed when calling lwip
                    // Make a DNS request for the MQTT server IP address
                    for (int i=0; i < DNS_MAX_SERVERS; i++)
                    {
                        printf("mqtt: dns server [%d] is  <%s>\n", i, ipaddr_ntoa(dns_getserver(i)));
                    }
                    ip_addr_t dns_addr;
                    ipaddr_aton("8.8.8.8", &dns_addr);
                    dns_setserver(0, &dns_addr);

                    printf("mqtt: resolve <%s> with server <%s>\n", MQTT_SERVER, ipaddr_ntoa(dns_getserver(0)));

                    cyw43_arch_lwip_begin();
                    int err = dns_gethostbyname(MQTT_SERVER, &rmqtt.mqtt_server_address, dns_found, &rmqtt);
                    cyw43_arch_lwip_end();

                    if (err == ERR_INPROGRESS) {
                        rmqtt.dns_requested = true;
                    }
                }
            }
            return;
        }

        if (!rmqtt.client_starting) {
            start_client(&rmqtt);
        }
        return;
        //if (!state.connect_done || mqtt_client_is_connected(state.mqtt_client_inst)) 
    }
}

