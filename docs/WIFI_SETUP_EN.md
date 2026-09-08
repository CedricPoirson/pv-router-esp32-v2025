# First Wi-Fi setup — PV Router V14.6

This procedure applies to the **TTGO T-Display** used by this PV Router branch.

Wi-Fi setup mode is **never started automatically** just because the home router or network is unavailable. A deliberate physical button action is required during boot.

## Enter setup mode

1. Power the PV Router off.
2. Hold the TTGO user button (`GPIO35`).
3. Power the PV Router back on while keeping the button pressed.
4. Keep holding it for about **3 seconds** until `MODE CONFIG WIFI` appears.
5. Release the button.

The TTGO then shows:

```text
MODE CONFIG WIFI
Wi-Fi : PVRouter-Setup
Password : pvrouter14
Then open:
http://192.168.4.1
```

## Connect from a phone or computer

Join this access point:

```text
SSID     : PVRouter-Setup
Password : pvrouter14
```

Your phone may automatically open the captive portal. Otherwise open manually:

```text
http://192.168.4.1
```

The page provides a bilingual FR/EN form and a list of detected Wi-Fi networks.

Enter:

- your home Wi-Fi SSID;
- your Wi-Fi password.

Then press **Save and restart**.

The credentials are stored in:

```text
/wifi.json
```

on SPIFFS. Stored credentials then **take priority over the compile-time `config.h` credentials**.

The PV Router restarts automatically, disables the `PVRouter-Setup` access point and attempts to connect to the new network.

## Normal boot

If the button is not held during boot, the PV Router starts normally.

Wi-Fi credential priority is:

```text
1. /wifi.json when it contains a real SSID
2. WIFI_NETWORK / WIFI_PASSWORD from src/config/config.h as fallback
```

The historical `xxx` value simply means “no valid SPIFFS Wi-Fi configuration”.

## If the home Wi-Fi is unavailable

The firmware waits up to `WIFI_TIMEOUT` (20 s in the reference configuration), then continues booting in Wi-Fi-offline mode and periodically retries the connection in the background.

The TTGO shows, among other messages:

```text
WI-FI INDISPONIBLE
Boot + bouton 3 s = config
```

The setup access point is **not started automatically**. This prevents an ordinary home-router outage from exposing a maintenance network.

To change Wi-Fi, deliberately reboot the PV Router while holding the button for 3 seconds.

## Safety during setup mode

Setup mode is a dedicated maintenance mode: normal regulation tasks are not started until credentials have been saved and the PV Router has rebooted.

The setup access point uses WPA2 with password `pvrouter14` and only exists after the physical boot-button action.

## SPIFFS and `uploadfs`

`pio run -t uploadfs` replaces the SPIFFS filesystem and can overwrite `/wifi.json`.

Before an `uploadfs`, keep a copy of Wi-Fi settings if required, or plan to run the physical setup procedure again afterwards.

The template file remains:

```text
data/wifi.json.ori
```

but V14.6 no longer requires editing this file manually just to change Wi-Fi: the setup portal can be used directly from a phone.

## Button behavior

The setup portal uses the **same GPIO35 button** used for screen navigation during normal operation.

- during boot: hold 3 s → Wi-Fi setup;
- during normal operation: short press → next screen page;
- during normal operation: long press → screen OFF.

No second button is required for this feature.
