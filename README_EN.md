# PV Router ESP32 / TTGO T-Display — Fronius Zero Grid + RobotDyn

🇫🇷 **[Documentation française](./README.md)** | 🇬🇧 **English — this file**

Photovoltaic surplus router for **ESP32 / TTGO T-Display**, using real-time measurements from a **Fronius inverter + Smart Meter** and HTTP control of a **RobotDyn Wi-Fi dimmer** driving a resistive water-heater load.

The goal is to consume as much PV surplus locally as possible while keeping the grid exchange close to zero, with a deliberate slight export bias to reduce short grid imports during fast load or PV changes.

> **PV Router firmware: V14.4**  
> **Zero Grid control algorithm: V14.3**  
> **Fronius interface: Solar API v1**  
> **Fronius endpoint used: `/solar_api/v1/GetPowerFlowRealtimeData.fcgi`**  
> **Tested RobotDyn dimmer firmware: `Version 20260514`**

![TTGO Router](./img/routeur.jpg)

---

## Origin, credits and acknowledgements

This project is an **adaptation and evolution of the ESP32 PV Router by xlyric / C_Lyric**:

- original PV Router project: https://github.com/xlyric/pv-router-esp32
- upstream author / maintainer: **xlyric / C_Lyric**
- community associated with the original project: **APPER**

Many thanks to **xlyric / C_Lyric** for publishing and maintaining this work as open source, and to the APPER community contributors. This repository would not exist in its current form without that foundation.

This branch also uses the Wi-Fi dimmer project from the same author:

- **PV-discharge-Dimmer-AC-Dimmer-KIT-Robotdyn**: https://github.com/xlyric/PV-discharge-Dimmer-AC-Dimmer-KIT-Robotdyn

The dimmer used here is a **RobotDyn / D1 mini** controlled over HTTP. The RobotDyn firmware validated with this branch is `Version 20260514`.

This adaptation intentionally differs from the original PV Router in several important areas: the **Fronius Smart Meter, read through Fronius Solar API v1, is now the regulation source of truth**, the active control loop no longer uses the SCT013 as its primary measurement source, the Zero Grid controller is predictive, and the Web UI / TTGO display have been extensively redesigned.

> This repository is a personal adaptation of the upstream project and should not be presented as an official release from xlyric or the APPER association.

---

## 1. Architecture

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

## 2. Tested hardware

Reference setup used for this project:

- ESP32 **TTGO T-Display**, 240 × 135, ST7789 controller;
- **Fronius Primo 6.0-1** inverter;
- **Fronius Smart Meter TS 65A-1**;
- **RobotDyn / D1 mini** Wi-Fi dimmer;
- tested RobotDyn firmware: `Version 20260514`;
- BTA16 triac in the currently used dimmer hardware;
- Dallas / DS18B20 probe connected to the RobotDyn for DHW temperature;
- resistive water-heater load calibrated in the PV Router to **800 W**;
- optional MQTT / Home Assistant telemetry.

The repository still contains some legacy code from the former local CT measurement path, but this branch is designed and maintained around **Fronius Solar API v1 measurement**.

---

# Fronius: Zero Grid source of truth

## 3. Fronius API used

The firmware directly queries:

```text
GET http://<IP_FRONIUS>/solar_api/v1/GetPowerFlowRealtimeData.fcgi
```

This is the **Fronius Solar API v1** real-time PowerFlow endpoint. This branch **does not use MQTT, Modbus or Home Assistant as the regulation measurement source**.

A JSON sample is accepted only if:

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

Example:

```text
P_PV   = 4800 W
P_Grid = -1200 W
```

means approximately 4.8 kW PV production and 1.2 kW exported to the grid.

### Fronius timing

```text
PowerFlow polling       : every 1.5 s
HTTP timeout            : 700 ms
Sample considered stale : after 4 s
```

Each validated Fronius sample increments a sample counter. The controller makes **at most one regulation decision per new validated sample**, preventing repeated calculations from the same measurement.

---

# Zero Grid regulation V14.3

## 4. Target and defaults

Default V14.3 settings:

```text
Heater resistance power : 800 W
Maximum dimmer           : 100 %
Grid target              : -15 W
Deadband                 : ±10 W
Effective target band    : -25 W to -5 W
```

A small amount of export is therefore deliberately kept.

These values can now be changed from Web V2 and are stored in `config.json`:

```json
{
  "heater_power_w": 800,
  "grid_target_w": -15,
  "grid_deadband_w": 10,
  "dimmer_max_percent": 100
}
```

Older `config.json` files remain compatible: if these keys are missing, the defaults above are used.

## 5. Control calculation

`P_Grid` already includes the effect of the heater whenever it is running. The router therefore starts from the heater power currently applied and computes the correction required to bring the grid toward the target.

Physical approximation:

```text
Pheater_target = Pheater_current + (Pgrid_target - Pgrid_current)
```

Example with an 800 W calibrated load:

```text
Actual dimmer        : 50 %  -> ~400 W
P_Grid               : -200 W
Grid target          : -15 W
Correction           : +185 W
Target heater power  : ~585 W
Target dimmer        : ~73 %
```

V14.3 no longer relies on arbitrary `+3/+8/+20 %` upward ramps. It moves directly toward the physics-based target, with a predictive bound preventing the controller from requesting more additional heater power than the currently visible export can safely provide.

## 6. Grid-import reaction

Grid import has priority. Internal thresholds currently are:

```text
Fast import threshold      : 80 W
Emergency import threshold : 250 W
Fast reserve               : 25 W
Emergency reserve          : 50 W
```

Downward power changes are not ramp-limited. If a large appliance starts in the house, the heater can be shed immediately.

## 7. Load saturation

If the dimmer reaches its configured maximum and PV surplus remains:

```text
LOAD LIMITED
```

The router cannot physically absorb any more power, so the remaining surplus is exported to the grid.

---

# RobotDyn

## 8. Power command

The PV Router sends an **absolute dimmer command**:

```text
GET http://<IP_DIMMER>/?POWER=<0..100>
```

Examples:

```text
/?POWER=0
/?POWER=25
/?POWER=100
```

With an 800 W physically calibrated heater:

```text
1 %   ~= 8 W
50 %  ~= 400 W
100 % ~= 800 W
```

The RobotDyn `/config.charge` value may be different: **the regulation uses the physical heater power configured in the PV Router**, not the RobotDyn `charge` value automatically.

## 9. RobotDyn real-time state

```text
GET http://<IP_DIMMER>/state
```

Example:

```json
{
  "dimmer": 0,
  "commande": 0,
  "temperature": "55.5",
  "power": 0,
  "Ptotal": 0,
  "RSSI": -59,
  "version": "Version 20260514",
  "onoff": true,
  "alerte": "RAS",
  "dallas0": "55.5"
}
```

The PV Router prefers `dallas0` for DHW temperature and falls back to `temperature` if needed.

Polling cadence:

```text
Dimmer catching up / unsynchronised : ~2 s
Dimmer synchronised                 : ~5 s
HTTP timeout                        : 500 ms
```

The command is refreshed every **60 s**, comfortably below the RobotDyn 5-minute auto-off.

A significant `CMD / ACTUAL` mismatch lasting about 15 s causes the command to be resent.

## 10. RobotDyn thermal configuration

The PV Router also reads:

```text
GET http://<IP_DIMMER>/config
```

Important fields are:

```text
maxtemp
trigger
minpow
maxpow
charge
```

`maxtemp` is the normal RobotDyn thermal setpoint and takes priority over the PV Router fallback `tmax`.

`trigger` is the thermal hysteresis expressed as a percentage of `maxtemp`.

The tested RobotDyn firmware uses integer arithmetic and effectively applies:

```text
release = maxtemp - ((maxtemp * trigger) / 100)
```

with integer truncation.

Current example:

```text
maxtemp = 56 °C
trigger = 3 %

(56 * 3) / 100 = 1
release = 55 °C
```

The PV Router mirrors **the exact same formula**. After `TEMP MAX` is reached, heating stays inhibited (`TEMP HOLD`) until the release temperature is reached.

If `/config` is unavailable, a conservative **2 °C** fallback hysteresis is temporarily used.

`/config` polling cadence:

```text
After success       : every 30 s
Until first success : retry every 10 s
```

---

# Safety / fail-safe

## 11. Conditions forcing POWER=0

The PV Router requests `POWER=0` when:

- the Fronius is unreachable;
- the latest Fronius sample is older than 4 s;
- `autonome=false`;
- RobotDyn reports `onoff=false`;
- a non-temperature RobotDyn alarm is active;
- the local DHW temperature protection is active.

A failed fail-safe request is retried quickly. An acknowledged safety command is also refreshed periodically.

**MQTT and Home Assistant are not part of this safety chain.**

---

# TTGO display

## 12. Main dashboard

The TTGO dashboard shows, among other information:

- time;
- DHW temperature / Tmax;
- `CE OK`, `CE SYNC`, `TEMP MAX`, `TEMP HOLD` states;
- large `IMPORT`, `ZERO GRID`, `DISPO` or `SURPLUS` banner;
- PV production;
- grid import/export;
- water-heater percentage and power;
- estimated house consumption;
- release temperature while thermal hold is active;
- power gauge.

V14.4 uses a **differential renderer**: only fields whose visible value changes are redrawn, reducing periodic ST7789 flicker.

A short button press switches to the diagnostic page. A long press keeps the manual screen-off function.

## 13. Graphical boot screen

Startup shows:

```text
PV ROUTER
FW V14.4 | ZERO GRID V14.3
```

with a vector energy-flow illustration:

```text
sun -> house -> water heater -> grid
```

and staged Wi-Fi, configuration, Web server and ready states.

---

# Web V2

## 14. Dashboard

```text
http://<ROUTER_IP>/
```

The dashboard is self-contained and does not require a CDN. It shows:

- PV / grid / house / heater measurements;
- Fronius / RobotDyn / MQTT states;
- DHW temperature and Tmax;
- V14.3 regulation target, band, correction and CMD / ACTUAL;
- browser-side history for about 30 minutes;
- copyable diagnostic report;
- free heap and uptime.

Graph history stays in the browser and does not continuously write to ESP32 flash.

## 15. Configuration

```text
http://<ROUTER_IP>/config.html
```

Currently editable V14 settings:

```text
RobotDyn IP address
Screen timeout
Fallback DHW Tmax
Actual heater resistance power
Grid target
Deadband
Maximum dimmer percentage
```

Sensitive fast-control thresholds intentionally remain firmware constants.

## 16. Local PV Router Web API

### Full status

```text
GET /api/status
```

The JSON explicitly exposes:

```text
version / firmware_version = V14.4
fronius_api                 = Solar API v1
fronius_powerflow_path      = /solar_api/v1/GetPowerFlowRealtimeData.fcgi
regulation.version          = V14.3
```

plus PV, grid, house, heater, DHW, RobotDyn, MQTT, uptime and memory information.

### Configuration

```text
GET  /api/config
POST /api/config
```

### Backup / restore

```text
GET  /api/config/export
POST /api/config/import
```

### Screen / restart

```text
POST /api/screen/toggle
POST /api/restart
```

---

# MQTT / Home Assistant

## 17. MQTT

Main state topic:

```text
pvrouter/state
```

Availability topic:

```text
pvrouter/availability
```

Published data includes:

- PV production;
- grid power;
- estimated house consumption;
- available power;
- water-heater power;
- dimmer command;
- actual dimmer;
- DHW temperature and Tmax;
- RSSI;
- Fronius state;
- RobotDyn state;
- dimmer synchronisation state.

Home Assistant Discovery is available when `HA_ENABLED` is enabled.

---

# Installation / HOW TO

## 18. Clone the repository

```bash
git clone <REPOSITORY_URL>
cd pv-router-esp32-v2025
git checkout feature/fronius-zero-grid-v13-dimmer-20260514
```

The project uses **PlatformIO**.

## 19. Create `src/config/config.h`

This local file contains secrets and is not meant to be committed:

```bash
cp src/config/config.example.h src/config/config.h
```

At minimum, configure:

```cpp
#define WIFI_NETWORK "MY_WIFI"
#define WIFI_PASSWORD "MY_PASSWORD"

#define IP_FRONIUS "192.168.x.x"

#define MQTT_SERVER "192.168.x.x"
#define MQTT_PORT 1883
#define MQTT_USER "my_user"
#define MQTT_PASSWORD "my_password"
```

To disable MQTT:

```cpp
#define MQTT_CLIENT false
```

To enable Home Assistant Discovery:

```cpp
#define HA_ENABLED true
```

### Wi-Fi through SPIFFS

If `WIFI_PASSWORD` is intentionally set to `"xxx"`, the firmware attempts to use `/wifi.json` from SPIFFS. Otherwise the compile-time credentials from `config.h` are used.

## 20. Prepare SPIFFS for a first installation

```bash
cp data/config.json.ori data/config.json
cp data/wifi.json.ori data/wifi.json
```

In `data/config.json`, verify at least:

```json
{
  "autonome": false,
  "dimmer": "192.168.100.29",
  "tmax": 65,
  "screentime": 0,
  "heater_power_w": 800,
  "grid_target_w": -15,
  "grid_deadband_w": 10,
  "dimmer_max_percent": 100
}
```

`autonome=false` is recommended for the first flash. Set it to `true` only after Fronius and RobotDyn communication have been validated.

## 21. Build

```bash
pio run
```

GitHub Actions CI also builds the TTGO firmware on the working branch / pull request.

## 22. Flash firmware

```bash
pio run -t upload
```

Then open the serial monitor:

```bash
pio device monitor -b 115200
```

## 23. Flash SPIFFS

For the first deployment or after changing `data/index.html`, `data/config.html`, etc.:

```bash
pio run -t uploadfs
```

### IMPORTANT: `uploadfs` replaces the filesystem

`config.json` can therefore be lost if it is not backed up first.

Recommended Web V2 procedure:

1. open `http://<ROUTER_IP>/config.html`;
2. click **Exporter config.json**;
3. keep the downloaded file;
4. only then run `uploadfs`;
5. restore it with **Importer config.json** if needed.

CLI backup of the complete file:

```bash
curl --connect-timeout 5 http://<ROUTER_IP>/api/config/export -o data/config.json
pio run -t uploadfs
rm data/config.json
```

Do not save `/api/config` as `data/config.json`: `/api/config` is only a simplified Web configuration view, while `/api/config/export` returns the **complete SPIFFS configuration file**.

---

# Post-flash verification

## 24. Expected logs

Healthy startup example:

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

Fronius must be ONLINE before routing is allowed.

## 25. Test Fronius directly

From a browser or `curl`:

```text
http://<IP_FRONIUS>/solar_api/v1/GetPowerFlowRealtimeData.fcgi
```

Check at least:

```text
Head.Status.Code = 0
Body.Data.Site.P_Grid
Body.Data.Site.P_PV
```

## 26. Test RobotDyn directly

```bash
curl http://<IP_DIMMER>/state
curl http://<IP_DIMMER>/config
curl 'http://<IP_DIMMER>/?POWER=0'
```

To change the thermal trigger directly on the tested RobotDyn firmware:

```text
http://<IP_DIMMER>/get?trigger=3&save=1
```

then verify with:

```text
http://<IP_DIMMER>/config
```

---

# Quick diagnostics

## 27. `FRONIUS OFFLINE`

Check:

- `IP_FRONIUS`;
- ESP32-to-Fronius LAN connectivity;
- that Solar API v1 responds;
- that `P_Grid` exists and `Head.Status.Code == 0`.

The fail-safe must keep `POWER=0` while Fronius data is not fresh.

## 28. `DIMMER OFFLINE`

Check:

```text
http://<IP_DIMMER>/state
```

and the address configured in Web V2 / `config.json`.

## 29. Missing temperature

`/state` must contain:

```text
dallas0
```

or, as a fallback:

```text
temperature
```

Displayed temperature is intentionally cleared when RobotDyn communication becomes stale, so an old temperature is never shown as live data.

## 30. `TEMP MAX` / `TEMP HOLD`

- `TEMP MAX`: water temperature has reached or exceeded `maxtemp`;
- `TEMP HOLD`: temperature has fallen below Tmax but has not yet reached the release temperature derived from `trigger`;
- `REPRISE xx°C` / release temperature is shown on the TTGO.

## 31. No routing despite PV surplus

Check in this order:

1. Fronius ONLINE;
2. RobotDyn ONLINE;
3. `autonome=true`;
4. `onoff=true`;
5. no non-temperature alarm;
6. no `TEMP HOLD`;
7. `P_Grid` becomes negative when exporting.

---

# OTA

## 32. Web OTA update

```text
http://<ROUTER_IP>/update
```

The current firmware uses `AsyncElegantOTA`.

**At present, the active `/update` route has no authentication configured.** It must therefore be considered accessible to devices that can reach the router on the LAN. The historical `otapassword` field in `config.json` is not applied to this active route.

---

# Project structure

## 33. Main files

```text
src/main.cpp
    initialisation and FreeRTOS task creation

src/config/version.h
    firmware / Zero Grid / Fronius API versions

src/tasks/measure-electricity.h
    Fronius Solar API v1 acquisition

src/tasks/Dimmer.h
    watchdog and one regulation action per fresh Fronius sample

src/functions/froniusZeroGrid.h
    Zero Grid V14.3 algorithm and RobotDyn POWER commands

src/tasks/gettemp.h
    RobotDyn /state + /config, Dallas, Tmax, trigger and hysteresis

src/tasks/updateDisplay.h
    TTGO drawing primitives / legacy renderer

src/tasks/smoothDisplay.h
    active V14.4 differential TTGO renderer

src/tasks/bootScreen.h
    graphical vector boot screen

src/functions/webFunctions.h
    Web V2 and local /api/* endpoints

src/functions/Mqtt_http_Functions.h
    MQTT telemetry and Home Assistant Discovery

src/functions/spiffsFunctions.h
    config.json and wifi.json read/write

data/index.html
    Web V2 dashboard

data/config.html
    Web V2 configuration page
```

---

# Versions

## 34. V14.4 — current firmware

V14.4 currently includes:

- fast physics-based **Zero Grid V14.3** regulation;
- Web-configurable regulation parameters;
- configurable physical heater power;
- RobotDyn `maxtemp` + `trigger` reading and matching hysteresis behavior;
- Web V2 dashboard, 30-minute graph and diagnostics;
- full `config.json` backup / restore;
- enhanced TTGO display;
- graphical boot screen;
- differential TTGO rendering to reduce flicker;
- separate and explicit firmware, control-algorithm and Fronius-API versions.

## 35. Version summary

```text
PV Router firmware       : V14.4
Zero Grid algorithm      : V14.3
Fronius interface        : Solar API v1
Tested RobotDyn firmware : Version 20260514
```

This separation is intentional: the **PV Router firmware** version can change for Web UI or display improvements without changing the Zero Grid control algorithm or the Fronius API generation.
