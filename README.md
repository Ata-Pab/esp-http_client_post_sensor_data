> **Note:** This project includes two implementations:
> - **`master` branch** ([http_client_post_sensor_data](https://github.com/Ata-Pab/esp-http_client_post_sensor_data)): Plain HTTP implementation
> - This **`secure_https_client_post_sensor_data` branch**: Secure HTTPS implementation


## Project Overview: `secure_https_client_post_sensor_data`

1. **SNTP time sync** — essential for HTTPS certificate validation (TLS requires accurate system time).
2. **Static CA PEM with `esp_crt_bundle_attach()`**, meaning the ESP32 now uses Espressif’s built-in trusted CA store — just like browsers do.

---

### Improvements made on top of the ```http_client_post_sensor_data``` project

| Area                   | Old approach         | New approach                                  | Why it matters                                                            |
| ---------------------- | -------------------- | --------------------------------------------- | ------------------------------------------------------------------------- |
| **Time handling**      | No SNTP              | Added NTP time sync                           | Certificates are time-bound; ESP must know the correct date/time.         |
| **TLS trust store**    | Custom CA PEM        | Espressif CA bundle (`esp_crt_bundle_attach`) | No manual certificate updates — works with any valid public HTTPS server. |
| **Wi-Fi reconnection** | Basic reconnect loop | Semaphore-based, state-tracked                | Thread-safe and cooperative with FreeRTOS tasks.                          |
| **Error handling**     | Retry logic          | Queue + reconnection request                  | Reliable for unstable connections.                                        |

## SNTP + TLS

```
     ┌──────────────────────────────┐
     │         HTTPS Server         │
     │  (has certificate signed by  │
     │   Let's Encrypt / DigiCert)  │
     └───────────────┬──────────────┘
                     │
           TLS Handshake (Verify cert)
                     │
             ┌───────┴────────┐
             │  ESP32 Client  │
             │  + SNTP Time   │
             │  + CA Bundle   │
             └────────────────┘
```

If SNTP hasn’t set the ```current time``` yet, ESP thinks it’s year **1970**, and all certificates appear “not yet valid,”
causing:

```
E (xxxx) esp-tls-mbedtls: mbedtls_ssl_handshake returned -0x2700
```

Now, synchronize time before HTTPS, certificate validation will succeed.


## Implementation Checks

1. **Make sure SNTP has finished sync before the HTTPS request.**
   ```c
   while (!s_time_synced && retry < retry_count)
   ```
2. **Confirm ESP-IDF build includes the CA bundle.**
   `sdkconfig`:

   ```
   CONFIG_ESP_TLS_USING_MBEDTLS=y
   CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y
   CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_FULL=y
   ```

   → If not, run `idf.py menuconfig → Component config → mbedTLS → Certificate Bundle`.

3. **Don’t call `wifi_manager_request_reconnect()` on TLS failure anymore**,
   since now the failure is not related to Wi-Fi.


## Serial Output Example

```
I (4823) wifi_manager: Got IP: 192.168.1.15
I (4833) wifi_manager: Initializing SNTP
I (6850) wifi_manager: Time synchronized via SNTP
I (6850) wifi_manager: Current time: 2025-11-09 14:20:31
I (6860) http_client: HTTPS POST Status = 200, content_length = 34
```

## Project Concept Summary

```
Wi-Fi Manager (FreeRTOS Task)
     ↓ gives semaphore
SNTP Time Sync
     ↓ ensures clock valid
HTTP(S) Client Task
     ↓ uses esp_crt_bundle for TLS
Server Communication
```

---
