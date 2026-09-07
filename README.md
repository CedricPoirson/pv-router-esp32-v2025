# PV Router ESP32 / TTGO T-Display — Fronius Zero Grid + RobotDyn

Routeur de surplus photovoltaïque pour **ESP32 / TTGO T-Display**, basé sur la mesure temps réel d'un **Fronius + Smart Meter** et le pilotage HTTP d'un **dimmer Wi-Fi RobotDyn** alimentant un chauffe-eau résistif.

Le but est de consommer localement le surplus PV tout en restant au plus près de zéro au point de livraison, avec un léger biais volontaire vers l'export pour limiter les micro-imports lors des variations rapides de charge ou de production.

> **Firmware PV Router : V14.4**  
> **Algorithme de régulation Zero Grid : V14.3**  
> **Interface Fronius : Solar API v1**  
> **Endpoint Fronius utilisé : `/solar_api/v1/GetPowerFlowRealtimeData.fcgi`**  
> **Dimmer RobotDyn testé : firmware `Version 20260514`**

![Routeur TTGO](./img/routeur.jpg)

---

## 1. Architecture

```text
                         réseau 230 V
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
 chauffe-eau / résistance
```

La **boucle de régulation ne dépend ni de MQTT ni de Home Assistant**. Le chemin critique est uniquement :

```text
Fronius -> ESP32 -> RobotDyn
```

MQTT, Home Assistant, le dashboard Web et l'écran sont des couches de télémétrie / interface.

---

## 2. Matériel testé

Configuration de référence du projet :

- ESP32 **TTGO T-Display** 240 × 135, contrôleur ST7789 ;
- onduleur **Fronius Primo 6.0-1** ;
- **Fronius Smart Meter TS 65A-1** ;
- dimmer Wi-Fi **RobotDyn / D1 mini** ;
- firmware RobotDyn testé : `Version 20260514` ;
- triac BTA16 sur le montage actuellement utilisé ;
- sonde Dallas / DS18B20 côté RobotDyn pour la température ECS ;
- chauffe-eau résistif calibré dans le PV Router à **800 W** ;
- MQTT / Home Assistant facultatifs.

Le dépôt contient encore des éléments hérités de l'ancienne mesure locale par transformateur de courant, mais cette branche est conçue et maintenue autour de la **mesure Fronius Solar API v1**.

---

# Fronius : source de vérité du Zero Grid

## 3. API Fronius utilisée

Le firmware interroge directement :

```text
GET http://<IP_FRONIUS>/solar_api/v1/GetPowerFlowRealtimeData.fcgi
```

Il s'agit de la **Fronius Solar API v1**, endpoint PowerFlow temps réel. Cette branche **ne base pas la régulation sur MQTT, Modbus ou Home Assistant**.

Le JSON est accepté uniquement si :

- la requête HTTP retourne 200 ;
- le JSON est valide ;
- `Head.Status.Code == 0` ;
- `Body.Data.Site.P_Grid` existe ;
- `P_Grid` est une valeur finie et reste dans une plage cohérente.

La production PV est lue dans :

```text
Body.Data.Site.P_PV
```

avec repli éventuel sur :

```text
Body.Data.Inverters.1.P
```

### Convention de signe utilisée

```text
P_Grid > 0  = import depuis le réseau
P_Grid < 0  = export vers le réseau
```

Exemple :

```text
P_PV   = 4800 W
P_Grid = -1200 W
```

signifie environ 4,8 kW de production et 1,2 kW exportés.

### Cadence Fronius

```text
Lecture PowerFlow     : toutes les 1,5 s
Timeout HTTP          : 700 ms
Donnée considérée stale : après 4 s
```

Chaque mesure Fronius validée incrémente un compteur d'échantillon. La régulation ne prend **qu'une décision par nouvel échantillon validé**, ce qui évite de recalculer plusieurs fois à partir de la même donnée.

---

# Régulation Zero Grid V14.3

## 4. Objectif et valeurs par défaut

Les réglages V14.3 par défaut sont :

```text
Puissance résistance        : 800 W
Dimmer maximum              : 100 %
Cible réseau                : -15 W
Bande morte                 : ±10 W
Bande cible effective       : -25 W à -5 W
```

Une légère exportation est donc volontairement conservée.

Ces valeurs sont maintenant configurables depuis l'interface Web V2 et stockées dans `config.json` :

```json
{
  "heater_power_w": 800,
  "grid_target_w": -15,
  "grid_deadband_w": 10,
  "dimmer_max_percent": 100
}
```

Les anciens `config.json` restent compatibles : si ces clés sont absentes, les valeurs ci-dessus sont utilisées.

## 5. Principe de calcul

`P_Grid` contient déjà l'effet de la résistance si elle chauffe. Le routeur part donc de la puissance réellement appliquée au chauffe-eau, puis calcule la correction nécessaire pour amener le réseau vers la cible.

Approximation physique utilisée :

```text
Pheater_cible = Pheater_actuel + (Pcible_reseau - Pgrid_actuel)
```

Exemple, résistance calibrée à 800 W :

```text
Dimmer réel       : 50 %  -> ~400 W
P_Grid            : -200 W
Cible réseau      : -15 W
Correction        : +185 W
Puissance CE cible: ~585 W
Dimmer cible      : ~73 %
```

V14.3 ne repose plus sur des rampes arbitraires `+3/+8/+20 %`. La montée suit directement le surplus calculé, avec une borne prédictive empêchant de demander davantage que l'export réellement visible.

## 6. Réaction aux imports

L'import réseau est prioritaire. Les seuils internes actuels sont :

```text
Import rapide      : 80 W
Import urgence     : 250 W
Réserve rapide     : 25 W
Réserve urgence    : 50 W
```

La baisse de puissance n'est pas limitée par une rampe : si un appareil démarre dans la maison, le routeur peut délester immédiatement la résistance.

## 7. Saturation de la charge

Si le dimmer atteint sa limite maximale et qu'il reste encore du surplus :

```text
LOAD LIMITED
```

Le routeur ne peut physiquement plus absorber davantage ; le surplus restant est exporté.

---

# RobotDyn

## 8. Commande de puissance

Le PV Router envoie une **consigne absolue** :

```text
GET http://<IP_DIMMER>/?POWER=<0..100>
```

Exemples :

```text
/?POWER=0
/?POWER=25
/?POWER=100
```

Avec une résistance réellement calibrée à 800 W :

```text
1 %   ~= 8 W
50 %  ~= 400 W
100 % ~= 800 W
```

La valeur `/config.charge` du RobotDyn peut être différente : **la régulation utilise la puissance physique configurée dans le PV Router**, pas automatiquement la valeur `charge` du RobotDyn.

## 9. État temps réel RobotDyn

```text
GET http://<IP_DIMMER>/state
```

Exemple :

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

Le PV Router utilise en priorité `dallas0` pour l'eau ECS, puis `temperature` en secours.

Cadence :

```text
Dimmer en rattrapage / désynchronisé : ~2 s
Dimmer synchronisé                   : ~5 s
Timeout HTTP                         : 500 ms
```

La commande est également rafraîchie toutes les **60 s**, notamment pour rester très en dessous de l'auto-off RobotDyn de 5 minutes.

Une désynchronisation importante `CMD / ACTUAL` maintenue environ 15 s provoque un renvoi de la commande.

## 10. Configuration thermique RobotDyn

Le PV Router lit aussi :

```text
GET http://<IP_DIMMER>/config
```

Les champs importants sont :

```text
maxtemp
trigger
minpow
maxpow
charge
```

`maxtemp` est la consigne thermique normale du RobotDyn et reste prioritaire sur le `tmax` de secours du PV Router.

`trigger` correspond à l'hystérésis thermique en pourcentage de `maxtemp`.

Le firmware RobotDyn testé travaille avec des entiers et applique effectivement :

```text
reprise = maxtemp - ((maxtemp * trigger) / 100)
```

avec troncature entière.

Exemple actuel :

```text
maxtemp = 56 °C
trigger = 3 %

(56 * 3) / 100 = 1
reprise = 55 °C
```

Le PV Router reproduit **exactement la même formule**. Une fois `TEMP MAX` atteint, il maintient la chauffe bloquée (`TEMP HOLD`) jusqu'à la température de reprise.

Si `/config` n'est pas disponible, un repli conservateur de **2 °C** est utilisé temporairement.

Cadence `/config` :

```text
Après succès : toutes les 30 s
Tant que non disponible : nouvelle tentative toutes les 10 s
```

---

# Sécurités / fail-safe

## 11. Conditions qui imposent POWER=0

Le PV Router force une demande `POWER=0` si :

- le Fronius est inaccessible ;
- la dernière mesure Fronius a plus de 4 s ;
- `autonome=false` ;
- le RobotDyn remonte `onoff=false` ;
- une alarme RobotDyn non thermique est active ;
- la protection thermique ECS locale est active.

Une tentative de fail-safe échouée est retentée rapidement. Une commande de sécurité déjà acquittée est périodiquement rafraîchie.

**MQTT et Home Assistant ne font pas partie de cette chaîne de sécurité.**

---

# Écran TTGO

## 12. Dashboard principal

Le dashboard affiche notamment :

- heure ;
- température ECS / Tmax ;
- état `CE OK`, `CE SYNC`, `TEMP MAX`, `TEMP HOLD` ;
- grand bandeau `IMPORT`, `ZERO GRID`, `DISPO` ou `SURPLUS` ;
- production PV ;
- import/export réseau ;
- pourcentage et puissance chauffe-eau ;
- consommation maison estimée ;
- température de reprise en cas de blocage thermique ;
- jauge de puissance.

L'affichage V14.4 utilise un **rendu différentiel** : seules les zones dont la valeur change sont redessinées, afin d'éviter le scintillement périodique du ST7789.

Un appui court bascule vers la page de diagnostic. Un appui long conserve la fonction d'extinction manuelle.

## 13. Écran de boot

Le démarrage est graphique et montre :

```text
PV ROUTER
FW V14.4 | ZERO GRID V14.3
```

avec un flux d'énergie vectoriel :

```text
soleil -> maison -> chauffe-eau -> réseau
```

et les étapes Wi-Fi, configuration, serveur Web et prêt.

---

# Interface Web V2

## 14. Dashboard

```text
http://<IP_DU_ROUTEUR>/
```

Le dashboard est autonome et ne dépend d'aucun CDN. Il fournit :

- mesures PV / réseau / maison / chauffe-eau ;
- état Fronius / RobotDyn / MQTT ;
- température ECS et Tmax ;
- régulation V14.3 : cible, bande, correction, CMD / ACTUAL ;
- historique navigateur sur environ 30 minutes ;
- diagnostic copiable ;
- mémoire libre et uptime.

L'historique du graphe reste dans le navigateur : il n'écrit pas en permanence dans la flash de l'ESP32.

## 15. Configuration

```text
http://<IP_DU_ROUTEUR>/config.html
```

Paramètres V14 actuellement éditables :

```text
Adresse IP RobotDyn
Timeout écran
Tmax ECS de secours
Puissance réelle de la résistance
Cible réseau
Bande morte
Limite maximale du dimmer
```

Les réglages sensibles internes de réaction rapide restent volontairement dans le firmware.

## 16. API Web locale du PV Router

### État complet

```text
GET /api/status
```

Le JSON expose notamment :

```text
version / firmware_version = V14.4
fronius_api               = Solar API v1
fronius_powerflow_path    = /solar_api/v1/GetPowerFlowRealtimeData.fcgi
regulation.version        = V14.3
```

ainsi que PV, réseau, maison, chauffe-eau, ECS, RobotDyn, MQTT, uptime et mémoire.

### Configuration

```text
GET  /api/config
POST /api/config
```

### Sauvegarde / restauration

```text
GET  /api/config/export
POST /api/config/import
```

### Écran / redémarrage

```text
POST /api/screen/toggle
POST /api/restart
```

---

# MQTT / Home Assistant

## 17. MQTT

Topic d'état :

```text
pvrouter/state
```

Disponibilité :

```text
pvrouter/availability
```

Le firmware publie notamment :

- production PV ;
- réseau ;
- consommation maison estimée ;
- puissance disponible ;
- puissance chauffe-eau ;
- consigne dimmer ;
- dimmer réel ;
- température et Tmax ECS ;
- RSSI ;
- état Fronius ;
- état RobotDyn ;
- synchronisation.

Home Assistant Discovery est disponible si `HA_ENABLED` est activé.

---

# Installation / HOW TO

## 18. Récupérer le dépôt

```bash
git clone <URL_DU_DEPOT>
cd pv-router-esp32-v2025
git checkout feature/fronius-zero-grid-v13-dimmer-20260514
```

Le projet utilise **PlatformIO**.

## 19. Créer `src/config/config.h`

Le fichier local contient les secrets et n'est pas destiné à être versionné :

```bash
cp src/config/config.example.h src/config/config.h
```

À renseigner au minimum :

```cpp
#define WIFI_NETWORK "MON_WIFI"
#define WIFI_PASSWORD "MON_MOT_DE_PASSE"

#define IP_FRONIUS "192.168.x.x"

#define MQTT_SERVER "192.168.x.x"
#define MQTT_PORT 1883
#define MQTT_USER "mon_user"
#define MQTT_PASSWORD "mon_password"
```

Pour désactiver MQTT :

```cpp
#define MQTT_CLIENT false
```

Pour Home Assistant Discovery :

```cpp
#define HA_ENABLED true
```

### Wi-Fi via SPIFFS

Si `WIFI_PASSWORD` vaut volontairement `"xxx"`, le firmware tente d'utiliser `/wifi.json` dans SPIFFS. Sinon les identifiants de `config.h` sont utilisés.

## 20. Préparer SPIFFS pour une première installation

```bash
cp data/config.json.ori data/config.json
cp data/wifi.json.ori data/wifi.json
```

Dans `data/config.json`, vérifier au minimum :

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

`autonome=false` est conseillé lors du premier flash. Passer à `true` uniquement après avoir validé la communication Fronius et RobotDyn.

## 21. Compiler

```bash
pio run
```

La CI GitHub Actions compile également le firmware TTGO sur la branche / PR.

## 22. Flasher le firmware

```bash
pio run -t upload
```

Puis :

```bash
pio device monitor -b 115200
```

## 23. Flasher SPIFFS

Pour un premier déploiement ou après modification de `data/index.html`, `data/config.html`, etc. :

```bash
pio run -t uploadfs
```

### IMPORTANT : `uploadfs` remplace le filesystem

`config.json` peut donc être perdu si on ne le sauvegarde pas avant.

La méthode recommandée depuis Web V2 est :

1. ouvrir `http://<IP_DU_ROUTEUR>/config.html` ;
2. cliquer **Exporter config.json** ;
3. conserver le fichier ;
4. seulement ensuite faire `uploadfs` ;
5. restaurer le fichier avec **Importer config.json** si nécessaire.

En CLI, le bon endpoint de sauvegarde complète est :

```bash
curl --connect-timeout 5 http://<IP_DU_ROUTEUR>/api/config/export -o data/config.json
pio run -t uploadfs
rm data/config.json
```

Ne pas sauvegarder `/api/config` comme `data/config.json` : `/api/config` est une vue simplifiée de la configuration Web, alors que `/api/config/export` renvoie le **vrai fichier SPIFFS complet**.

---

# Vérification après flash

## 24. Logs attendus

Exemple de démarrage sain :

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

Le Fronius doit être ONLINE avant d'autoriser le routage.

## 25. Tester directement le Fronius

Dans un navigateur ou avec `curl` :

```text
http://<IP_FRONIUS>/solar_api/v1/GetPowerFlowRealtimeData.fcgi
```

Vérifier notamment :

```text
Head.Status.Code = 0
Body.Data.Site.P_Grid
Body.Data.Site.P_PV
```

## 26. Tester directement le RobotDyn

```bash
curl http://<IP_DIMMER>/state
curl http://<IP_DIMMER>/config
curl 'http://<IP_DIMMER>/?POWER=0'
```

Pour modifier le trigger thermique directement sur le firmware RobotDyn testé :

```text
http://<IP_DIMMER>/get?trigger=3&save=1
```

puis vérifier avec :

```text
http://<IP_DIMMER>/config
```

---

# Diagnostic rapide

## 27. `FRONIUS OFFLINE`

Vérifier :

- l'IP `IP_FRONIUS` ;
- que l'ESP32 et le Fronius peuvent communiquer sur le LAN ;
- que la Solar API v1 répond ;
- que `P_Grid` est présent et que `Head.Status.Code == 0`.

Le fail-safe doit maintenir `POWER=0` tant que la donnée Fronius n'est pas fraîche.

## 28. `DIMMER OFFLINE`

Vérifier :

```text
http://<IP_DIMMER>/state
```

et l'adresse configurée dans Web V2 / `config.json`.

## 29. Température absente

`/state` doit contenir :

```text
dallas0
```

ou, à défaut :

```text
temperature
```

La température affichée est volontairement effacée lorsque la communication RobotDyn devient périmée, afin de ne pas présenter une ancienne mesure comme actuelle.

## 30. `TEMP MAX` / `TEMP HOLD`

- `TEMP MAX` : la température a atteint ou dépassé `maxtemp` ;
- `TEMP HOLD` : la température est redescendue sous Tmax mais n'a pas encore atteint la température de reprise calculée depuis `trigger` ;
- `REPRISE xx°C` est visible sur le TTGO.

## 31. Pas de routage malgré un surplus

Vérifier dans cet ordre :

1. Fronius ONLINE ;
2. RobotDyn ONLINE ;
3. `autonome=true` ;
4. `onoff=true` ;
5. aucune alarme non thermique ;
6. pas de `TEMP HOLD` ;
7. `P_Grid` devient bien négatif en export.

---

# OTA

## 32. Mise à jour OTA Web

```text
http://<IP_DU_ROUTEUR>/update
```

La version actuelle utilise `AsyncElegantOTA`.

**À ce jour, aucune authentification n'est appliquée à `/update` dans le firmware actif.** L'interface doit donc être considérée comme accessible aux appareils ayant accès au LAN. Le champ historique `otapassword` de `config.json` n'est pas appliqué à cette route actuelle.

---

# Structure du projet

## 33. Fichiers principaux

```text
src/main.cpp
    initialisation et création des tâches FreeRTOS

src/config/version.h
    versions firmware / Zero Grid / API Fronius

src/tasks/measure-electricity.h
    acquisition Fronius Solar API v1

src/tasks/Dimmer.h
    watchdog et déclenchement de la régulation une fois par échantillon Fronius

src/functions/froniusZeroGrid.h
    algorithme Zero Grid V14.3 et commandes POWER RobotDyn

src/tasks/gettemp.h
    /state + /config RobotDyn, Dallas, Tmax, trigger et hystérésis

src/tasks/updateDisplay.h
    primitives / ancien moteur d'affichage TTGO

src/tasks/smoothDisplay.h
    rendu différentiel TTGO actif V14.4

src/tasks/bootScreen.h
    boot graphique vectoriel

src/functions/webFunctions.h
    Web V2 et API locale /api/*

src/functions/Mqtt_http_Functions.h
    télémétrie MQTT et Home Assistant Discovery

src/functions/spiffsFunctions.h
    lecture / écriture config.json et wifi.json

data/index.html
    dashboard Web V2

data/config.html
    page de configuration V2
```

---

# Versions

## 34. V14.4 — firmware actuel

V14.4 regroupe notamment :

- régulation physique rapide **Zero Grid V14.3** ;
- paramètres de régulation configurables Web ;
- puissance réelle de résistance configurable ;
- lecture `maxtemp` + `trigger` RobotDyn et hystérésis identique au firmware RobotDyn ;
- dashboard Web V2, graphe 30 min et diagnostic ;
- sauvegarde / restauration complète de `config.json` ;
- écran TTGO enrichi ;
- boot graphique ;
- rendu différentiel TTGO pour réduire le scintillement ;
- version firmware / version régulation / version API Fronius séparées et explicites.

## 35. Résumé des versions à ne pas confondre

```text
PV Router firmware      : V14.4
Zero Grid algorithm     : V14.3
Fronius interface       : Solar API v1
RobotDyn firmware testé : Version 20260514
```

Cette séparation est volontaire : une évolution de l'interface Web ou de l'écran peut faire évoluer le **firmware PV Router** sans modifier l'algorithme de régulation Zero Grid ni l'API du Fronius.
