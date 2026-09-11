# Prévision Solcast V15 — Home Assistant → MQTT → PV Router

Cette fonction est **strictement informative**. La régulation Zero Grid reste locale et indépendante de Home Assistant, MQTT et Solcast.

Architecture :

```text
Solcast -> Home Assistant -> MQTT pvrouter/forecast -> PV Router -> TTGO
```

## Prérequis

- intégration Solcast installée dans Home Assistant ;
- broker MQTT fonctionnel ;
- PV Router V15.0 ou plus récent ;
- entité Solcast disposant de l'attribut `detailedForecast`.

Configuration de référence testée :

```text
sensor.solcast_pv_forecast_previsions_pour_aujourd_hui
```

L'attribut doit contenir des points de 30 minutes avec au minimum :

```text
period_start
pv_estimate
pv_estimate10
pv_estimate90
```

## Installation de l'automatisation

Le fichier prêt à l'emploi est :

```text
home-assistant/pvrouter_solcast_automation.yaml
```

Si Home Assistant utilise `automations.yaml`, copier le bloc complet tel quel à la fin du fichier. Il commence volontairement par `- id:`.

Si le nom de l'entité Solcast diffère, remplacer les occurrences de :

```text
sensor.solcast_pv_forecast_previsions_pour_aujourd_hui
```

puis vérifier la configuration et recharger les automatisations.

## Calcul envoyé au routeur

L'automatisation publie sur :

```text
pvrouter/forecast
```

avec `retain: true`.

Exemple :

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

Les heures de fin correspondent à la **fin du pas Solcast de 30 minutes**. Par exemple, si le dernier point >= 2,5 kW commence à 17:30, la fenêtre se termine à 18:00.

## Signification du pictogramme

Le pictogramme décrit la **qualité de la journée solaire pour le routeur**, et non la météo générale :

```text
sunny    = journée solaire forte et régulière
variable = production exploitable mais plus irrégulière
cloudy   = faible potentiel solaire
```

La classification de référence utilise :

- `cloudy` si le pic prévu reste sous 1,5 kW ou si aucune période n'atteint 2 kW ;
- `sunny` si le pic atteint au moins 4 kW, la fenêtre >= 2,5 kW dure au moins 4 h et aucun creux marqué n'est détecté ;
- `variable` dans les autres cas.

## Fraîcheur et sécurité

`valid_until` est un timestamp Unix UTC. Le routeur compare cette valeur avec une heure UTC réelle, indépendamment de l'affichage CET/CEST du TTGO.

Deux sécurités empêchent l'affichage d'une vieille prévision :

- expiration absolue via `valid_until` ;
- TTL local de 8 h depuis le dernier message MQTT reçu.

Avant la première synchronisation NTP, le routeur conserve temporairement la prévision reçue et s'appuie uniquement sur le TTL local. Dès qu'une heure UTC plausible est disponible, `valid_until` est appliqué normalement.

Un message :

```json
{"valid":false}
```

efface explicitement la prévision.

## Diagnostic

Dans Home Assistant, écouter le topic :

```text
pvrouter/forecast
```

Le log série du routeur doit afficher par exemple :

```text
[MQTT] subscribed pvrouter/forecast
[FORECAST] sunny | 2.5k 10:00>18:00 | 2.0k 09:30>18:30
[FORECAST] diag reason=ok age=0 s valid_until=1789060693 utc=1789053493 expires_in=7200 s
```

Le champ `reason` permet de distinguer immédiatement :

```text
ok                  = prévision fraîche
waiting_ntp         = NTP pas encore synchronisé, TTL local utilisé
no_data             = aucune prévision valide
local_ttl_expired   = dernier MQTT reçu depuis plus de 8 h
valid_until_expired = timestamp absolu dépassé
```

Si le message MQTT est correct mais n'apparaît pas sur le TTGO, vérifier en priorité `reason`, `expires_in`, la synchronisation NTP et la version du firmware.

## Affichage TTGO

Exemple :

```text
☀  2.5k 10>18
   2k   9:30>18:30
```

Le pictogramme est dessiné dans la partie droite du grand bandeau. Les deux fenêtres occupent la zone inférieure droite de l'écran principal.

`TEMP HOLD` reste prioritaire dans l'en-tête et la régulation Zero Grid n'utilise jamais la prévision Solcast.
