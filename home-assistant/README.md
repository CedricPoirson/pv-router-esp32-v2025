# Home Assistant integration

## Solcast forecast for PV Router V15

Ready-to-use automation:

```text
pvrouter_solcast_automation.yaml
```

It reads the Solcast `detailedForecast` attribute, computes the useful solar windows above 2.0 kW and 2.5 kW, classifies the solar day (`sunny`, `variable`, `cloudy`) and publishes a retained JSON message to:

```text
pvrouter/forecast
```

Documentation:

- French: [`../docs/SOLCAST_FR.md`](../docs/SOLCAST_FR.md)
- English: [`../docs/SOLCAST_EN.md`](../docs/SOLCAST_EN.md)

The forecast is informational only. Home Assistant and MQTT are never part of the Zero Grid control loop or its fail-safe.
