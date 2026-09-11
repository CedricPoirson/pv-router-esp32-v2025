#ifndef PV_ROUTER_SOLAR_FORECAST_H
#define PV_ROUTER_SOLAR_FORECAST_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <NTPClient.h>

// Home Assistant remains responsible for talking to Solcast and reducing the
// forecast to a tiny, local MQTT payload. The PV Router only displays this
// information; it is deliberately excluded from the Zero Grid control loop.
static const char* PVROUTER_FORECAST_TOPIC = "pvrouter/forecast";

enum SolarForecastWeather : uint8_t {
  SOLAR_WEATHER_UNKNOWN = 0,
  SOLAR_WEATHER_SUNNY = 1,
  SOLAR_WEATHER_VARIABLE = 2,
  SOLAR_WEATHER_CLOUDY = 3
};

struct SolarForecastData {
  bool valid = false;
  SolarForecastWeather weather = SOLAR_WEATHER_UNKNOWN;
  String p2500Start;
  String p2500End;
  String p2000Start;
  String p2000End;
  uint32_t validUntilEpoch = 0;
  unsigned long receivedMs = 0;
  unsigned long revision = 0;
};

static SolarForecastData gSolarForecast;

// timeClient is defined in main.cpp. NTPClient includes the configured local
// offset in getEpochTime(), so keep the active Europe/Paris offset beside it in
// order to compare Home Assistant Unix timestamps against true UTC.
extern NTPClient timeClient;
extern long gNtpParisOffsetSeconds;

static bool solarForecastValidClock(const String &value)
{
  if (value.length() != 5 || value.charAt(2) != ':') return false;
  if (!isDigit(value.charAt(0)) || !isDigit(value.charAt(1)) ||
      !isDigit(value.charAt(3)) || !isDigit(value.charAt(4))) return false;

  const int hour = value.substring(0, 2).toInt();
  const int minute = value.substring(3, 5).toInt();
  return hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59;
}

static String solarForecastReadClock(JsonVariantConst value)
{
  if (value.isNull()) return String();
  const String text = value.as<String>();
  return solarForecastValidClock(text) ? text : String();
}

static SolarForecastWeather solarForecastWeatherFromString(String value)
{
  value.toLowerCase();
  value.replace("-", "_");
  value.replace(" ", "_");

  if (value == "sunny" || value == "clear" || value == "mostly_sunny") {
    return SOLAR_WEATHER_SUNNY;
  }
  if (value == "variable" || value == "partly_cloudy" ||
      value == "mostly_cloudy" || value == "mixed") {
    return SOLAR_WEATHER_VARIABLE;
  }
  if (value == "cloudy" || value == "overcast" || value == "hazy" ||
      value == "rain" || value == "rainy" || value == "fog") {
    return SOLAR_WEATHER_CLOUDY;
  }
  return SOLAR_WEATHER_UNKNOWN;
}

static int64_t solarForecastUtcNowEpoch()
{
  const int64_t localEpoch = (int64_t)timeClient.getEpochTime();
  return localEpoch - (int64_t)gNtpParisOffsetSeconds;
}

static long solarForecastAgeSeconds()
{
  if (gSolarForecast.receivedMs == 0) return -1;
  return (long)((unsigned long)(millis() - gSolarForecast.receivedMs) / 1000UL);
}

static long solarForecastExpiresInSeconds()
{
  if (gSolarForecast.validUntilEpoch == 0) return -1;

  const int64_t nowUtcEpoch = solarForecastUtcNowEpoch();
  if (nowUtcEpoch <= 1700000000LL) return -1;

  const int64_t remaining =
      (int64_t)gSolarForecast.validUntilEpoch - nowUtcEpoch;

  if (remaining > 2147483647LL) return 2147483647L;
  if (remaining < -2147483647LL) return -2147483647L;
  return (long)remaining;
}

static const char* solarForecastFreshnessReason()
{
  if (!gSolarForecast.valid || gSolarForecast.receivedMs == 0)
    return "no_data";

  if ((unsigned long)(millis() - gSolarForecast.receivedMs) >
      8UL * 60UL * 60UL * 1000UL)
    return "local_ttl_expired";

  if (gSolarForecast.validUntilEpoch > 0) {
    const int64_t nowUtcEpoch = solarForecastUtcNowEpoch();
    if (nowUtcEpoch <= 1700000000LL)
      return "waiting_ntp";
    if ((uint32_t)nowUtcEpoch > gSolarForecast.validUntilEpoch)
      return "valid_until_expired";
  }

  return "ok";
}

static bool solarForecastIsFresh()
{
  const char *reason = solarForecastFreshnessReason();

  // Until NTP has synchronized, keep the retained MQTT forecast available and
  // rely on the local eight-hour TTL. Once UTC becomes plausible,
  // valid_until is enforced normally.
  return strcmp(reason, "ok") == 0 || strcmp(reason, "waiting_ntp") == 0;
}

static String solarForecastWindowLine(const char *threshold,
                                      const String &start,
                                      const String &end)
{
  if (start.length() != 5 || end.length() != 5) {
    return String(threshold) + " --";
  }
  return String(threshold) + " " + start + ">" + end;
}

static bool handleSolarForecastPayload(const byte *payload, unsigned int length)
{
  if (!payload || length == 0 || length > 900) return false;

  // The project currently pins ArduinoJson 6.21.x, where JsonDocument itself
  // is abstract/protected. A fixed document avoids heap allocation and keeps
  // the incoming MQTT parser bounded.
  StaticJsonDocument<1024> doc;
  const DeserializationError err = deserializeJson(doc, payload, length);
  if (err) {
    Serial.printf("[FORECAST] invalid JSON: %s\n", err.c_str());
    return false;
  }

  JsonObjectConst root = doc.as<JsonObjectConst>();
  if (root["valid"].is<bool>() && !root["valid"].as<bool>()) {
    gSolarForecast.valid = false;
    gSolarForecast.receivedMs = millis();
    gSolarForecast.revision++;
    Serial.println("[FORECAST] cleared by Home Assistant");
    return true;
  }

  const String p2500Start = solarForecastReadClock(root["p2500_start"]);
  const String p2500End = solarForecastReadClock(root["p2500_end"]);
  const String p2000Start = solarForecastReadClock(root["p2000_start"]);
  const String p2000End = solarForecastReadClock(root["p2000_end"]);

  // Missing windows are valid (for example on a cloudy day), but a half-window
  // is rejected so the TTGO never displays a misleading interval.
  if ((p2500Start.length() == 0) != (p2500End.length() == 0) ||
      (p2000Start.length() == 0) != (p2000End.length() == 0)) {
    Serial.println("[FORECAST] rejected incomplete time window");
    return false;
  }

  const char *weatherText = root["weather"] | "unknown";
  gSolarForecast.p2500Start = p2500Start;
  gSolarForecast.p2500End = p2500End;
  gSolarForecast.p2000Start = p2000Start;
  gSolarForecast.p2000End = p2000End;
  gSolarForecast.weather = solarForecastWeatherFromString(String(weatherText));
  gSolarForecast.validUntilEpoch = root["valid_until"] | 0UL;
  gSolarForecast.receivedMs = millis();
  gSolarForecast.valid = true;
  gSolarForecast.revision++;

  Serial.printf("[FORECAST] %s | 2.5k %s>%s | 2.0k %s>%s\n",
                weatherText,
                gSolarForecast.p2500Start.c_str(),
                gSolarForecast.p2500End.c_str(),
                gSolarForecast.p2000Start.c_str(),
                gSolarForecast.p2000End.c_str());

  const int64_t nowUtcEpoch = solarForecastUtcNowEpoch();
  const long expiresIn = solarForecastExpiresInSeconds();
  Serial.printf("[FORECAST] diag reason=%s age=%ld s valid_until=%lu utc=%lld expires_in=%ld s\n",
                solarForecastFreshnessReason(),
                solarForecastAgeSeconds(),
                (unsigned long)gSolarForecast.validUntilEpoch,
                (long long)nowUtcEpoch,
                expiresIn);
  return true;
}

#endif
