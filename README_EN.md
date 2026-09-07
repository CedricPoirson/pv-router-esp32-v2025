# PV Router ESP32 / TTGO T-Display — Fronius Zero Grid + RobotDyn

🇫🇷 **[Documentation française](./README.md)** | 🇬🇧 **English — this file**

Photovoltaic surplus router for **ESP32 / TTGO T-Display**, using real-time measurements from a **Fronius inverter + Smart Meter** and HTTP control of a **RobotDyn Wi-Fi dimmer** driving a resistive water-heater load.

The goal is to consume PV surplus locally while keeping grid exchange as close to zero as possible, with a deliberate slight export bias to reduce short grid imports during fast load or PV changes.

> **PV Router firmware: V14.6**  
> **Zero Grid control algorithm: V14.3**  
> **Fronius interface: Solar API v1**  
> **Fronius endpoint: `/solar_api/v1/GetPowerFlowRealtimeData.fcgi`**  
> **Tested RobotDyn dimmer firmware: `Version 20260514`**

![TTGO Router](./img/routeur.jpg)

---

## Origin, credits and acknowledgements

This project is an **adaptation and evolution of the ESP32 PV Router by xlyric / C_Lyric**:

- upstream project: https://github.com/xlyric/pv-router-esp32
- upstream author / maintainer: **xlyric / C_Lyric**
- community associated with the original project: **APPER**

Many thanks to **xlyric / C_Lyric** for publishing and maintaining this work as open source, and to the APPER community contributors. This repository would not exist in its current form without that foundation.

The Wi-Fi dimmer used by this branch is also based on the same author's project:

- **PV-discharge-Dimmer-AC-Dimmer-KIT-Robotdyn**: https://github.com/xlyric/PV-discharge-Dimmer-AC-Dimmer-KIT-Robotdyn

The hardware used here is a **RobotDyn / D1 mini** controlled over HTTP. The RobotDyn firmware validated with this branch is `Version 20260514`.

This adaptation intentionally differs from the original PV Router in several major areas: the **Fronius Smart Meter read through Fronius Solar API v1 is the regulation source of truth**, the active loop no longer uses SCT013 as its primary measurement source, the Zero Grid controller is predictive, and the Web UI / TTGO display have been extensively redesigned.

> This repository is a personal adaptation of the upstream project and should not be presented as an official release from xlyric or the APPER association.

---

# 1. Architecture

```text
                         230 V grid
                              ^
                              |
Fronius + Smart Meter         |
        |                     |
        | Solar API v1        |
        | HTTP / PowerFlow    |
        v                     |
     ESP32 / TTGO             |
        |                     |
        | Zero Grid V14.3     |
        | HTTP POWER=0..100   |
        v                     |
 RobotDyn Wi-Fi Dimmer -------+
        |
        v
 water heater / resistor
```

The **regulation loop does not depend on MQTT or Home Assistant**. The critical path is only:

```text
Fronius -> ESP32 -> RobotDyn
```

MQTT, Home Assistant, the Web dashboard and the TTGO screen are telemetry / user-interface layers.

---

# 2. Reference hardware

Tested setup:

- ESP32 **TTGO T-Display**, 240 × 135, ST7789;
- **Fronius Primo 6.0-1** inverter;
- **Fronius Smart Meter TS 65A-1**;
- **RobotDyn / D1 mini** Wi-Fi dimmer;
- tested RobotDyn firmware: `Version 20260514`;
- BTA16 triac in the current reference dimmer hardware;
- Dallas / DS18B20 probe on the RobotDyn side for DHW temperature;
- resistive water-heater load calibrated in the PV Router to **800 W**;
- optional MQTT / Home Assistant telemetry.

The repository still contains some legacy local CT-measurement code, but this branch is designed and maintained around **Fronius Solar API v1 measurement**.

---

# 3. Fronius: Zero Grid source of truth

The firmware directly queries:

```text
GET http://<IP_FRONIUS>/solar_api/v1/GetPowerFlowRealtimeData.fcgi
```

This is the **Fronius Solar API v1** real-time PowerFlow endpoint. This branch does not use MQTT, Modbus or Home Assistant as the regulation measurement source.

A sample is accepted only when:

- HTTP returns 200;
- the JSON is valid;
- `Head.Status.Code == 0`;
- `Body.Data.Site.P_Grid` exists;
- `P_Grid` is finite and within a plausible range.

PV production is read from:

```text
Body.Data.Site.P_PV
```

with a fallback to:

```text
Body.Data.Inverters.1.P
```

### Sign convention

```text
P_Grid > 0  = grid import
P_Grid < 0  = grid export
```

Example: `P_PV=4800 W` and `P_Grid=-1200 W` means about 4.8 kW PV production and 1.2 kW exported.

### Fronius timing

```text
PowerFlow polling       : every 1.5 s
HTTP timeout            : 700 ms
Sample considered stale : after 4 s
```

Each validated Fronius sample increments a sample counter. The controller makes at most one regulation decision per new validated sample.

---

# 4. Zero Grid regulation V14.3

Default settings:

```text
Heater resistance power : 800 W
Maximum dimmer           : 100 %
Grid target              : -15 W
Deadband                 : ±10 W
Effective target band    : -25 W to -5 W
```

A small amount of export is deliberately kept.

These values are stored in `config.json` and can be changed from Web V2:

```json
{
  "heater_power_w": 800,
  "grid_target_w": -15,
  "grid_deadband_w": 10,
  "dimmer_max_percent": 100
}
```

Physical control principle:

```text
Pheater_target = Pheater_current + (Pgrid_target - Pgrid_current)
```

Example with an 800 W heater:

```text
Actual dimmer        : 50 %  -> ~400 W
P_Grid               : -200 W
Grid target          : -15 W
Correction           : +185 W
Target heater power  : ~585 W
Target dimmer        : ~73 %
```

V14.3 no longer relies on arbitrary upward ramps. Downward power changes remain unrestricted so a newly started household load can shed the heater quickly.

Current internal thresholds:

```text
Fast import threshold      : 80 W
Emergency import threshold : 250 W
Fast reserve               : 25 W
Emergency reserve          : 50 W
```

When the dimmer is already at its configured limit and surplus remains, state may become `LOAD LIMITED`: remaining energy is exported because no more heater load is available.

---

# 5. RobotDyn

## Power command

The PV Router sends an absolute command:

```text
GET http://<IP_DIMMER>/?POWER=<0..100>
```

Examples:

```text
/?POWER=0
/?POWER=25
/?POWER=100
```

With an 800 W calibrated heater:

```text
1 %   ~= 8 W
50 %  ~= 400 W
100 % ~= 800 W
```

The RobotDyn `/config.charge` value may be different: regulation uses the physical heater power configured in the PV Router.

## Real-time state

```text
GET http://<IP_DIMMER>/state
```

The PV Router reads fields such as `dimmer`, `commande`, `dallas0`, `temperature`, `RSSI`, `version`, `onoff` and `alerte`.

`dallas0` is preferred for DHW temperature, with `temperature` as fallback.

Timing:

```text
Dimmer catching up / unsynchronised : ~2 s
Dimmer synchronised                 : ~5 s
HTTP timeout                        : 500 ms
Command keepalive                   : 60 s
Reference RobotDyn auto-off         : 5 min
```

## Temperature, Tmax and trigger

The PV Router also reads:

```text
GET http://<IP_DIMMER>/config
```

Important fields: `maxtemp`, `trigger`, `minpow`, `maxpow`, `charge`.

RobotDyn `maxtemp` takes priority over the PV Router fallback `tmax`.

The tested RobotDyn firmware uses integer arithmetic and applies:

```text
release = maxtemp - ((maxtemp * trigger) / 100)
```

Example:

```text
maxtemp = 56 °C
trigger = 3 %
(56 * 3) / 100 = 1
release = 55 °C
```

The PV Router mirrors this exact formula. Once `TEMP MAX` is reached, heating remains inhibited (`TEMP HOLD`) until the release temperature is reached.

If `/config` is unavailable, a conservative 2 °C fallback is temporarily used.

---

# 6. Safety / fail-safe

The PV Router requests `POWER=0` when:

- Fronius is unreachable;
- the latest Fronius sample is older than 4 s;
- `autonome=false`;
- RobotDyn reports `onoff=false`;
- a non-temperature RobotDyn alarm is active;
- the local DHW temperature protection is active.

MQTT and Home Assistant are not part of this safety chain.

---

# 7. First Wi-Fi setup — V14.6

V14.6 adds a **physically triggered Wi-Fi setup mode**. It never appears just because the home router is unavailable.

## Enter setup mode

1. power the PV Router off;
2. hold the TTGO user button (`GPIO35`);
3. power the PV Router back on while keeping the button pressed;
4. hold for about **3 seconds**;
5. release when the display shows `MODE CONFIG WIFI`.

The TTGO shows:

```text
Wi-Fi : PVRouter-Setup
Password : pvrouter14
http://192.168.4.1
```

From a phone or computer join:

```text
SSID     : PVRouter-Setup
Password : pvrouter14
```

The captive portal may open automatically. Otherwise browse to:

```text
http://192.168.4.1
```

The FR/EN page lists detected Wi-Fi networks and lets you enter the home SSID and password. After **Save and restart**, credentials are stored in `/wifi.json` and the ESP32 reboots.

Credential priority is now:

```text
1. /wifi.json when a real SSID is stored
2. WIFI_NETWORK / WIFI_PASSWORD from config.h as fallback
```

The historical `xxx` value simply means no valid SPIFFS Wi-Fi configuration is present.

If the home network stays unavailable for `WIFI_TIMEOUT` (20 s by default), boot no longer blocks forever: the firmware continues in offline mode and retries in the background. The maintenance access point is not opened automatically.

The V14.6 setup portal currently configures **Wi-Fi only**. Fronius IP, MQTT and other settings remain configured as described below.

Dedicated guides:

- [Première configuration Wi-Fi — français](./docs/WIFI_SETUP_FR.md)
- [First Wi-Fi setup — English](./docs/WIFI_SETUP_EN.md)

---

# 8. TTGO display

The main dashboard shows time, DHW temperature/Tmax, heater state, the large `IMPORT` / `ZERO GRID` / `DISPO` / `SURPLUS` banner, PV, grid, heater, house and the power gauge.

Rendering is **differential**: only areas whose visible value changes are redrawn, reducing ST7789 flicker.

Normal-operation button behavior:

```text
Short press : main -> diagnostics -> help -> main
Long press  : screen OFF
```

The help page explains `T/Tmax`, `CE OK`, `TEMP MAX`, `TEMP HOLD`, `IMPORT`, `DISPO`, `SURPLUS`, `PV`, `EXP/IMP`, `CE % / W` and `REPRISE`.

At startup, the graphical boot screen shows the PV Router logo, versions, a vector energy-flow illustration and Wi-Fi / configuration / Web-server / ready stages.

---

# 9. Web V2

Dashboard:

```text
http://<ROUTER_IP>/
```

Configuration:

```text
http://<ROUTER_IP>/config.html
```

The dashboard provides:

- PV / grid / house / heater;
- Fronius / RobotDyn / MQTT states;
- DHW temperature / Tmax / trigger / release;
- Zero Grid target, band, correction and CMD / ACTUAL;
- about 30 minutes of browser-side history with no continuous flash writes;
- copyable diagnostics;
- free heap and uptime;
- TTGO screen explanation/help.

Currently editable Web V2 settings:

```text
RobotDyn IP address
Screen timeout
Fallback DHW Tmax
Actual heater resistance power
Grid target
Deadband
Maximum dimmer percentage
```

Local API:

```text
GET  /api/status
GET  /api/config
POST /api/config
GET  /api/config/export
POST /api/config/import
POST /api/screen/toggle
POST /api/restart
```

`/api/status` explicitly exposes:

```text
firmware_version       = V14.6
fronius_api            = Solar API v1
fronius_powerflow_path = /solar_api/v1/GetPowerFlowRealtimeData.fcgi
regulation.version     = V14.3
```

---

# 10. MQTT / Home Assistant

Main topic:

```text
pvrouter/state
```

Availability:

```text
pvrouter/availability
```

Published data includes PV, grid, house, available power, heater, dimmer command/actual, temperature/Tmax, RSSI and Fronius/RobotDyn states.

Home Assistant Discovery is available when `HA_ENABLED` is enabled.

---

# 11. Installation / HOW TO

## Clone the repository

```bash
git clone <REPOSITORY_URL>
cd pv-router-esp32-v2025
git checkout feature/fronius-zero-grid-v13-dimmer-20260514
```

The project uses **PlatformIO**.

## Create `src/config/config.h`

```bash
cp src/config/config.example.h src/config/config.h
```

Configure at least:

```cpp
#define WIFI_NETWORK "FALLBACK_WIFI"
#define WIFI_PASSWORD "FALLBACK_PASSWORD"

#define IP_FRONIUS "192.168.x.x"

#define MQTT_SERVER "192.168.x.x"
#define MQTT_PORT 1883
#define MQTT_USER "my_user"
#define MQTT_PASSWORD "my_password"
```

Compile-time Wi-Fi credentials are now **fallback values**: a valid `/wifi.json` saved by the V14.6 portal takes priority.

To disable MQTT:

```cpp
#define MQTT_CLIENT false
```

To enable Home Assistant Discovery:

```cpp
#define HA_ENABLED true
```

## Prepare SPIFFS

For a full first installation:

```bash
cp data/config.json.ori data/config.json
cp data/wifi.json.ori data/wifi.json
```

Verify at least in `config.json`:

```json
{
  "autonome": false,
  "dimmer": "192.168.x.x",
  "tmax": 65,
  "screentime": 0,
  "heater_power_w": 800,
  "grid_target_w": -15,
  "grid_deadband_w": 10,
  "dimmer_max_percent": 100
}
```

`autonome=false` is recommended for the first flash. Set it to `true` only after Fronius and RobotDyn communication have been validated.

`data/wifi.json` may remain with placeholders if you plan to use the physical V14.6 portal.

## Build and flash

```bash
pio run
pio run -t upload
pio device monitor -b 115200
```

To upload Web assets / SPIFFS:

```bash
pio run -t uploadfs
```

### Important: `uploadfs`

`uploadfs` replaces the SPIFFS filesystem and can overwrite both `config.json` **and `wifi.json`**.

Before a new `uploadfs`, export the real complete configuration file:

```bash
curl --connect-timeout 5 http://<ROUTER_IP>/api/config/export -o data/config.json
pio run -t uploadfs
rm data/config.json
```

From Web V2, use **Exporter config.json** before uploading and **Importer config.json** afterwards if required.

For Wi-Fi, either keep `/wifi.json` or simply run the physical setup portal again after `uploadfs`.

---

# 12. Post-flash checks

Expected logs:

```text
WiFi connected
IP address:
192.168.x.x
Loading configuration...
start Web server
[FRONIUS] ONLINE PV=... W GRID=... W
[DIMMER] CONFIG OK MAX=... C TRIGGER=...% RELEASE=... C
[DIMMER] LINK OK ACTUAL=...% CMD=...% TEMP=... C ...
[MQTT] Connecting...connected
```

Test Fronius:

```text
http://<IP_FRONIUS>/solar_api/v1/GetPowerFlowRealtimeData.fcgi
```

Check `Head.Status.Code == 0`, `Body.Data.Site.P_Grid` and preferably `P_PV`.

Test RobotDyn:

```bash
curl http://<IP_DIMMER>/state
curl http://<IP_DIMMER>/config
curl 'http://<IP_DIMMER>/?POWER=0'
```

Change the thermal trigger on the tested RobotDyn firmware:

```text
http://<IP_DIMMER>/get?trigger=3&save=1
```

---

# 13. Quick diagnostics

### `FRONIUS OFFLINE`

Check `IP_FRONIUS`, LAN connectivity, Solar API v1, `P_Grid` and `Head.Status.Code`.

### `DIMMER OFFLINE`

Check `http://<IP_DIMMER>/state` and the RobotDyn address configured in Web V2 / `config.json`.

### `TEMP MAX` / `TEMP HOLD`

- `TEMP MAX`: Tmax reached;
- `TEMP HOLD`: temperature is below Tmax but not yet down to the release threshold;
- `REPRISE xx°C` shows the calculated release threshold.

### No routing despite PV surplus

Check in this order: Fronius ONLINE, RobotDyn ONLINE, `autonome=true`, `onoff=true`, no alarm, no `TEMP HOLD`, then confirm `P_Grid` becomes negative during export.

### Wi-Fi lost

The firmware retries in the background. To deliberately change networks, reboot while holding GPIO35 for about 3 seconds.

---

# 14. OTA

```text
http://<ROUTER_IP>/update
```

The current firmware uses `AsyncElegantOTA`.

**No authentication is currently applied to `/update`.** This route must therefore be considered reachable by devices with LAN access. The historical `otapassword` field in `config.json` is not applied to the active route.

---

# 15. Main files

```text
src/main.cpp
    boot, physical setup mode and FreeRTOS task creation

src/config/version.h
    firmware / Zero Grid / Fronius API versions

src/functions/wifiSetupPortal.h
    PVRouter-Setup AP, captive portal and wifi.json save

src/tasks/wifi-connection.h
    Wi-Fi connection/reconnection and wifi.json priority

src/tasks/measure-electricity.h
    Fronius Solar API v1 acquisition

src/functions/froniusZeroGrid.h
    Zero Grid V14.3 algorithm and RobotDyn commands

src/tasks/gettemp.h
    RobotDyn /state + /config, Dallas, Tmax, trigger and hysteresis

src/tasks/smoothDisplay.h
    differential TTGO renderer

src/tasks/displayHelp.h
    third on-device help page

src/tasks/bootScreen.h
    graphical vector boot screen

src/functions/webFunctions.h
    Web V2 and local API

src/functions/Mqtt_http_Functions.h
    MQTT and Home Assistant Discovery

src/functions/spiffsFunctions.h
    config.json and wifi.json

data/index.html
    Web V2 dashboard

data/config.html
    Web V2 configuration
```

---

# 16. Versions

## V14.6 — current firmware

V14.6 mainly adds **physical Wi-Fi provisioning at boot**: 3-second button hold, `PVRouter-Setup` access point, captive portal at `192.168.4.1`, `wifi.json` persistence, stored-credential priority and non-blocking boot when home Wi-Fi is unavailable.

It keeps:

- **Zero Grid V14.3**;
- Fronius **Solar API v1** as regulation source of truth;
- RobotDyn HTTP + thermal safety;
- Web V2 and MQTT/HA;
- graphical boot screen;
- differential TTGO rendering and help page.

```text
PV Router firmware       : V14.6
Zero Grid algorithm      : V14.3
Fronius interface        : Solar API v1
Tested RobotDyn firmware : Version 20260514
```

This separation is intentional: Web UI, display or installation improvements can change the PV Router firmware version without changing the Zero Grid algorithm or the Fronius API generation.
