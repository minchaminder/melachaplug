#pragma once

#include "esphome.h"

#include <cctype>
#include <cmath>
#include <string>
#include <vector>

#if defined(USE_ESP32)
#include "esp_sntp.h"
#elif defined(USE_ESP8266)
#include "sntp.h"
#else
#include "lwip/apps/sntp.h"
#endif

#if defined(USE_ESP8266)
#include <ESP8266WiFi.h>
#elif defined(USE_ESP32_FRAMEWORK_ARDUINO)
#include <WiFi.h>
#endif

namespace MelachaPlugNTP {

static const char *const PUBLIC_NTP_0 = "0.pool.ntp.org";
static const char *const PUBLIC_NTP_1 = "1.pool.ntp.org";
static const char *const PUBLIC_NTP_2 = "2.pool.ntp.org";
static std::string active_servers[3];

std::string trim(const std::string &value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        start++;
    }

    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        end--;
    }

    return value.substr(start, end - start);
}

bool addServer(std::vector<std::string> &servers, const std::string &value) {
    if (servers.size() >= 3) {
        return false;
    }

    std::string server = trim(value);
    if (server.empty() || server == "0.0.0.0") {
        return false;
    }

    for (const auto &existing : servers) {
        if (existing == server) {
            return false;
        }
    }

    servers.push_back(server);
    return true;
}

std::string getGatewayServer() {
#if defined(USE_ESP8266) || defined(USE_ESP32_FRAMEWORK_ARDUINO)
    std::string gateway = WiFi.gatewayIP().toString().c_str();
    if (gateway == "0.0.0.0") {
        return "";
    }
    return gateway;
#else
    return "";
#endif
}

int octetFromNumber(float value) {
    if (std::isnan(value)) {
        return 0;
    }

    int octet = static_cast<int>(value + 0.5f);
    if (octet < 0) {
        return 0;
    }
    if (octet > 255) {
        return 255;
    }
    return octet;
}

std::string getSavedLocalServer() {
    int octet_1 = octetFromNumber(id(local_ntp_ip_1).state);
    int octet_2 = octetFromNumber(id(local_ntp_ip_2).state);
    int octet_3 = octetFromNumber(id(local_ntp_ip_3).state);
    int octet_4 = octetFromNumber(id(local_ntp_ip_4).state);

    if (octet_1 == 0 && octet_2 == 0 && octet_3 == 0 && octet_4 == 0) {
        return "";
    }

    return std::to_string(octet_1) + "." + std::to_string(octet_2) + "." + std::to_string(octet_3) + "." +
           std::to_string(octet_4);
}

std::vector<std::string> buildServerList() {
    std::vector<std::string> servers;
    std::string local = getSavedLocalServer();
    std::string gateway = getGatewayServer();

    if (id(local_ntp_first).state) {
        addServer(servers, local);
        addServer(servers, gateway);
        addServer(servers, PUBLIC_NTP_0);
        addServer(servers, PUBLIC_NTP_1);
        addServer(servers, PUBLIC_NTP_2);
    } else {
        addServer(servers, PUBLIC_NTP_0);
        addServer(servers, PUBLIC_NTP_1);
        if (!addServer(servers, local)) {
            addServer(servers, gateway);
        }
        addServer(servers, PUBLIC_NTP_2);
    }

    return servers;
}

void applyServers(const std::vector<std::string> &servers) {
#if defined(USE_ESP32)
    if (esp_sntp_enabled()) {
        esp_sntp_stop();
    }
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    for (size_t i = 0; i < servers.size(); i++) {
        active_servers[i] = servers[i];
        esp_sntp_setservername(i, active_servers[i].c_str());
    }
    esp_sntp_set_sync_interval(id(sntp_time).get_update_interval());
    esp_sntp_init();
#else
    sntp_stop();
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    for (size_t i = 0; i < servers.size(); i++) {
        active_servers[i] = servers[i];
        sntp_setservername(i, active_servers[i].c_str());
    }
    sntp_init();
    id(sntp_time).update();
#endif
}

void configureFromSaved() {
    applyServers(buildServerList());
}

}  // namespace MelachaPlugNTP
