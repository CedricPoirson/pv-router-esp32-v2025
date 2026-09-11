# Solcast forecast V15 — Home Assistant → MQTT → PV Router

This feature is **informational only**. Zero Grid regulation remains local and independent from Home Assistant, MQTT and Solcast.

Architecture:

```text
Solcast -> Home Assistant -> MQTT pvrouter/forecast -> PV Router -> TTGO
```

## Requirements

- Solcast integration installed in Home Assistant;
- working MQTT broker;
- PV Router V15.0 or newer;
- a Solcast entity exposing the `detailedForecast` attribute.

Reference entity used by the tested Home Assistant installation:

```text
sensor.solcast_pv_forecast_previsions_pour_aujourd_hui
```

The attribute must contain 30-minute points with at least:

```text
period_start
pv_estimate
pv_estimate10
pv_estimate90
```

## Installing the automation

The ready-to-use file is:

```text
home-assistant/pvrouter_solcast_automation.yaml
```

If Home Assistant uses `automations.yaml`, copy the complete block at the end of that file. It intentionally starts with `- id:`.

If your Solcast entity has a different name, replace every occurrence of:

```text
sensor.solcast_pv_forecast_previsions_pour_aujourd_hui
```

then validate the configuration and reload automations.

## Data sent to the router

The automation publishes to:

```text
pvrouter/forecast
```

with `retain: true`.

Example:

```json
{
  "weather": "sunny",
  "p2500_start": "10:00",
  "p2500_end": "18:00",
  "p2000_start": "09:30",
  "p2000_end": "18:30",
  "valid_until": 1789060693
}
```

End times correspond to the **end of the 30-minute Solcast period**. For example, if the last point >= 2.5 kW starts at 17:30, the displayed window ends at 18:00.

## Weather pictogram meaning

The pictogram describes the **quality of the solar day for the router**, not general weather conditions:

```text
sunny    = strong and stable solar day
variable = usable but more irregular production
cloudy   = low solar potential
```

Reference classification:

- `cloudy` when the expected peak stays below 1.5 kW or no period reaches 2 kW;
- `sunny` when peak power reaches at least 4 kW, the >= 2.5 kW window lasts at least 4 h, and no marked mid-day dip is detected;
- `variable` otherwise.

## Freshness and safety

`valid_until` is an absolute UTC Unix timestamp. The router compares it with true UTC time, independently from the TTGO CET/CEST display offset.

Two safeguards prevent an old forecast from remaining on screen:

- absolute expiry through `valid_until`;
- a local 8-hour TTL since the last received MQTT forecast.

Before the first NTP synchronization, the router temporarily keeps the received forecast and relies on the local TTL only. As soon as a plausible UTC time is available, `valid_until` is enforced normally.

Publishing:

```json
{"valid":false}
```

explicitly clears the forecast.

## Diagnostics

In Home Assistant, listen to the source topic:

```text
pvrouter/forecast
```

The router also publishes a retained compact diagnostic document to:

```text
pvrouter/forecast/diagnostic
```

Example:

```json
{
  "valid": true,
  "fresh": true,
  "reason": "ok",
  "weather": "sunny",
  "age_s": 0,
  "valid_until": 1789060693,
  "utc_now": 1789053493,
  "expires_in_s": 7200,
  "revision": 4,
  "p2500": {"start":"10:00","end":"18:00"},
  "p2000": {"start":"09:30","end":"18:30"}
}
```

The router serial log also contains for example:

```text
[MQTT] subscribed pvrouter/forecast
[FORECAST] sunny | 2.5k 10:00>18:00 | 2.0k 09:30>18:30
[FORECAST] diag reason=ok age=0 s valid_until=1789060693 utc=1789053493 expires_in=7200 s
```

The `reason` field identifies the freshness state directly:

```text
ok                  = fresh forecast
waiting_ntp         = NTP not synchronized yet, local TTL is used
no_data             = no valid forecast
local_ttl_expired   = last MQTT forecast is older than 8 h
valid_until_expired = absolute timestamp has expired
```

If MQTT data is correct but nothing appears on the TTGO, check `reason`, `expires_in_s`, NTP synchronization and the firmware version first.

## TTGO display

Example:

```text
☀  2.5k 10>18
   2k   9:30>18:30
```

The weather pictogram is drawn in the free right-hand side of the main coloured banner. The two forecast windows use the lower-right area of the main dashboard.

`TEMP HOLD` remains visible in the header and Zero Grid regulation never uses the Solcast forecast.
