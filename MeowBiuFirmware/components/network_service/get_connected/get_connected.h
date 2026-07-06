#ifndef GET_CONNECTED_H
#define GET_CONNECTED_H
#include "esp_err.h"
#include "esp_wifi_types.h"
#include "stdbool.h"
#include <stddef.h>

#define DEFAULT_SSID "psu-personal"
#define DEFAULT_PWD ""
#define DEFAULT_SCAN_METHOD WIFI_FAST_SCAN
#define DEFAULT_SORT_METHOD WIFI_CONNECT_AP_BY_SIGNAL
#define DEFAULT_AUTHMODE WIFI_AUTH_OPEN
#define DEFAULT_RSSI_5G_ADJUSTMENT 0

#define WIFI_SSID_MAX_LEN 32
#define WIFI_PASSWD_MAX_LEN 64

#ifdef __cplusplus
extern "C"
{
#endif
    extern char ssid[WIFI_SSID_MAX_LEN + 1];
    extern char pwd[WIFI_PASSWD_MAX_LEN + 1];

    typedef enum
    {
        WIFI_STATE_DISCONNECTED = 0,
        WIFI_STATE_CONNECTING,
        WIFI_STATE_CONNECTED
    } wifi_connection_state_t;


    typedef struct
    {
        wifi_connection_state_t state;
        char ip[40];
        int reason;
    } wifi_status_event_t;

    typedef struct
    {
        const char *payload;
        size_t length;
    } http_response_event_t;


    /* Initialize the get_connected service */
    esp_err_t get_connected_init(void);
    esp_err_t get_connected_start(void);
    
    /* WiFi configuration */
    void set_ssid(const char* new_ssid);
    void set_pwd(const char* new_pwd);
    bool is_connected(void);
    
    /* HTTP requests */
    esp_err_t get_request(const char* url, void (*response_callback)(const char* response));
    esp_err_t test_request(void);
    
    /* MAC address */
    char* get_mac(void);
    
#ifdef __cplusplus
}
#endif
#endif
