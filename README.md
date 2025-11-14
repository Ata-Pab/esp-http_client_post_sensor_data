## Project Overview: `http_client_post_sensor_data`

Connect ESP32 to a Wi-Fi network → periodically send JSON-formatted data (e.g. sensor or UART data) via HTTPS to a test server → handle reconnection and task synchronization safely with FreeRTOS mechanisms.

## ⚙️ Architecture

```
+---------------------------+
|        Wi-Fi Task         |
| - Connect to AP           |
| - Manage reconnections    |
| - Signal Network Ready    |
+-------------+-------------+
              |
              v
+---------------------------+
|     Data Producer Task    |
| - Generate sample data    |
| - Send via Queue          |
+-------------+-------------+
              |
              v
+---------------------------+
|     HTTP Client Task      |
| - Wait for Network Ready  |
| - Read from Queue         |
| - POST JSON to Server     |
+---------------------------+
```

---

## 📂 Project Structure

```
wifi_http_client_demo/
├── main/
│   ├── main.c
│   ├── wifi_manager.c
│   ├── wifi_manager.h
│   ├── http_client.c
│   ├── http_client.h
│   ├── CMakeLists.txt
├── sdkconfig.defaults
├── CMakeLists.txt
├── .gitignore
└── README.md
```

---

## FreeRTOS Features Used

| Feature                | Used For                                       |
| ---------------------- | ---------------------------------------------- |
| Queue                  | Send JSON messages between producer and client |
| Binary Semaphore       | Signal “Wi-Fi connected”                       |
| Task Notifications     | Notify reconnect events                        |
| Static Task Allocation | For critical system tasks (optional toggle)    |

---

## Configuration (menuconfig)

* Wi-Fi SSID & Password
* Server URL (e.g., `https://httpbin.org/post`)
* POST interval (seconds)
* Optional: enable static task allocation

---

## Serial Output Example

```
Wi-Fi connected. IP: 192.168.1.105
[Producer] Sent new data: {"temp": 25.6, "humidity": 42}
[HTTP] POST success (200)
[Producer] Sent new data: {"temp": 25.7, "humidity": 43}
[HTTP] POST success (200)
```

## Plain HTTP demo for ESP32 using FreeRTOS + ESP-IDF.

- Connects to configured Wi-Fi (edit SSID/PASS in `main/wifi_manager.c`)
- Producer task creates JSON messages every 5s and enqueues them
- HTTP task posts queued messages to http://httpbin.org/post (plain HTTP)
- Uses FreeRTOS Queue, semaphore for Wi-Fi ready, and task notifications

## Build & Flash

```bash
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```


## How it uses FreeRTOS features (quick mapping)
- **Queue**: `xMessageQueue` carries JSON payloads from Producer → HTTP task.
- **Semaphore**: `s_wifi_connected_sem` (in wifi_manager) signals the first "got IP" event to waiting task(s).
- **Task Notifications**: `xTaskNotifyGive(xHttpTaskHandle)` is used as an optional kick to immediately wake the HTTP task after enqueue.
- **(Optional) Static allocation**: The code uses dynamic creation by default; if you want, we can convert `xTaskCreate` → `xTaskCreateStatic` for `http_task` & `producer_task` and allocate stacks/TCBs statically — I can add that on request.

---

## How to test quickly (no server set up)
1. Edit `main/wifi_manager.c` and set `WIFI_SSID` and `WIFI_PASS`.
2. `idf.py build`
3. `idf.py -p <PORT> flash monitor`
4. Observe logs:
   - Wi-Fi connect, IP address
   - Producer enqueues JSON every 5s
   - HTTP task performs POST → httpbin will respond with 200

If you prefer to test against a local server, run a local httpbin or simple HTTP echo server and replace `HTTP_POST_URL`.

---

## Next actions (pick one)
- **Add TLS support** (HTTPS) to this project — showing certificate pinning or CA store verification.
- **Improve Reliability**: Persistent queue in NVS, exponential backoff, or static task allocation for production.
- **Add MQTT** next, reusing the queue/synchronization patterns.