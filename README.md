# PV Router ESP32 / TTGO T-Display — Fronius Zero Grid + RobotDyn

Routeur de surplus photovoltaïque basé sur **ESP32 / TTGO T-Display**.

La version actuelle du projet récupère directement la puissance réseau auprès d'un **onduleur Fronius / Smart Meter**, calcule en temps réel la puissance disponible et pilote un **dimmer Wi-Fi RobotDyn** pour envoyer le surplus vers un chauffe-eau électrique.

L'objectif est de rester au plus près de **0 W réseau** tout en conservant une très légère exportation afin d'éviter les petits imports dus aux variations rapides de charge.

> Version actuelle de la logique Zero Grid : **V13.4**  
> Dimmer ECS testé : **firmware 20260514**  
> Charge actuellement calibrée : **800 W**

![Routeur TTGO](./img/routeur.jpg)

---

## 1. Fonctionnement

Le principe de la version Fronius est simple :

```text
Fronius / Smart Meter
        |
        | GetPowerFlowRealtimeData.fcgi
        v
      ESP32
        |
        | calcul Zero Grid
        v
RobotDyn Wi-Fi Dimmer
        |
        v
Chauffe-eau ECS
```

Le Fronius fournit notamment :

- `P_Grid` : puissance échangée avec le réseau ;
- `P_PV` : production photovoltaïque.

Convention utilisée par le firmware :

```text
P_Grid > 0  = import réseau
P_Grid < 0  = export réseau
```

Le contrôleur vise actuellement :

```text
Cible réseau : -10 W
Zone morte   : -20 W à 0 W
```

Cela laisse volontairement quelques watts d'export au lieu de chercher exactement `0 W` à chaque mesure.

---

## 2. Pourquoi la régulation tient compte du dimmer réel

La puissance réseau mesurée par le Fronius contient déjà la consommation du chauffe-eau.

Le firmware ne fait donc pas simplement :

```text
surplus solaire -> pourcentage dimmer
```

Il part de la puissance **réellement appliquée** au chauffe-eau et ajoute ou retire la correction nécessaire.

Exemple avec une charge de 800 W :

```text
Dimmer réel       : 50 %  -> environ 400 W
Export réseau     : 200 W
Nouvelle cible    : environ 600 W
Nouvelle consigne : environ 75 %
```

Cette méthode évite de sous-estimer le surplus lorsque le chauffe-eau absorbe déjà une partie de la production.

---

## 3. Matériel utilisé / testé

Configuration actuelle :

- ESP32 / **TTGO T-Display** ;
- onduleur **Fronius** avec Smart Meter accessible sur le réseau local ;
- dimmer AC Wi-Fi **RobotDyn / D1 mini** ;
- firmware du dimmer ECS : `Version 20260514` ;
- triac BTA16 sur le montage actuellement utilisé ;
- sonde Dallas raccordée au dimmer pour la température ECS ;
- chauffe-eau / charge résistive actuellement limitée à **800 W** ;
- MQTT / Home Assistant facultatif pour la télémétrie.

Le projet historique peut également fonctionner avec la mesure locale par transformateur / SCT013. Cette branche est cependant principalement consacrée au fonctionnement **Fronius Zero Grid**.

---

## 4. Sécurité

Ce projet commande une charge secteur et peut être utilisé sur un chauffe-eau.

- Le logiciel ne remplace pas les protections électriques matérielles.
- Utiliser un disjoncteur, une protection différentielle et un câblage adaptés.
- Conserver les sécurités thermiques du chauffe-eau.
- Toute intervention sur le 230 V doit être réalisée hors tension et par une personne compétente.

En cas de perte de données Fronius ou d'état de sécurité anormal, le firmware demande au dimmer de revenir à `POWER=0`.

---

# Installation / HOW TO

## 5. Prérequis logiciel

Le projet utilise **PlatformIO**.

Exemple avec VS Code + extension PlatformIO ou PlatformIO CLI.

Cloner le dépôt puis se placer dans son répertoire :

```bash
git clone <URL_DU_DEPOT>
cd pv-router-esp32-v2025
```

La configuration PlatformIO par défaut cible :

```text
ttgo-t-display
```

Le fichier `platformio.ini` contient actuellement un `upload_port` adapté à la machine de développement du projet. Si le port série est différent sur votre ordinateur, modifier ou supprimer cette ligne.

---

## 6. Créer la configuration de compilation

Le fichier contenant les identifiants personnels n'est pas destiné à être versionné.

Créer `src/config/config.h` depuis l'exemple :

```bash
cp src/config/config.example.h src/config/config.h
```

Modifier ensuite au minimum :

```cpp
#define WIFI_NETWORK "MON_WIFI"
#define WIFI_PASSWORD "MON_MOT_DE_PASSE"

#define IP_FRONIUS "192.168.x.x"

#define MQTT_SERVER "192.168.x.x"
#define MQTT_PORT 1883
#define MQTT_USER "mon_user"
#define MQTT_PASSWORD "mon_password"
```

Si MQTT n'est pas utilisé :

```cpp
#define MQTT_CLIENT false
```

Pour activer les fonctions Home Assistant prévues par cette branche :

```cpp
#define HA_ENABLED true
```

---

## 7. Configuration SPIFFS

Créer les fichiers de configuration à partir des modèles :

```bash
cp data/config.json.ori data/config.json
cp data/wifi.json.ori data/wifi.json
```

### `data/config.json`

Les paramètres les plus importants pour la branche Fronius sont :

```json
{
  "autonome": false,
  "dimmer": "192.168.100.29",
  "tmax": 65
}
```

### `autonome`

```text
false = routage volontairement désactivé
true  = régulation Zero Grid autorisée
```

Le modèle fourni garde volontairement `autonome=false` pour éviter qu'une nouvelle installation commence à router avant d'avoir été vérifiée.

### `dimmer`

Adresse IP du dimmer RobotDyn.

Cette même adresse est maintenant utilisée :

- pour lire `/state` ;
- pour envoyer les commandes `/?POWER=...`.

Il n'est donc plus nécessaire de maintenir deux adresses différentes dans le code.

### `tmax`

Température maximale ECS utilisée par les fonctions de télémétrie / état.

---

## 8. Compiler

```bash
pio run
```

La branche dispose également d'une CI GitHub Actions qui compile le firmware TTGO à chaque push / pull request.

---

## 9. Flasher le firmware

```bash
pio run -t upload
```

Puis, pour un premier déploiement ou lorsque les fichiers SPIFFS ont changé :

```bash
pio run -t uploadfs
```

Ouvrir ensuite le moniteur série :

```bash
pio device monitor -b 115200
```

---

## 10. Vérification au démarrage

Un démarrage normal doit montrer notamment :

```text
WiFi connected
[FRONIUS] ONLINE PV=... W GRID=... W
[DIMMER] LINK OK ACTUAL=...% CMD=...% TEMP=... C RSSI=...
```

Sur le TTGO, les informations principales sont :

- état Fronius ;
- production PV ;
- puissance réseau ;
- pourcentage ECS ;
- température ECS.

---

# API utilisées

## 11. Fronius

Endpoint principal :

```text
GET http://<IP_FRONIUS>/solar_api/v1/GetPowerFlowRealtimeData.fcgi
```

Le firmware utilise une seule acquisition Fronius pour alimenter :

- la régulation Zero Grid ;
- l'écran ;
- MQTT / Home Assistant ;
- les états de diagnostic.

Paramètres actuels :

```text
Intervalle de lecture : 1,5 s
Timeout HTTP          : 700 ms
Donnée périmée après  : 4 s
```

Une mesure n'est utilisée que si le JSON est valide, si `Status.Code == 0` et si `P_Grid` est cohérent.

---

## 12. RobotDyn ECS

### Envoyer une consigne absolue

```text
GET http://<IP_DIMMER>/?POWER=<0..100>
```

Exemples :

```text
POWER=0    -> arrêt
POWER=25   -> 25 %
POWER=100  -> pleine puissance
```

Avec la charge actuelle de 800 W :

```text
1 % ~= 8 W
50 % ~= 400 W
100 % ~= 800 W
```

### Lire l'état

```text
GET http://<IP_DIMMER>/state
```

Exemple de réponse :

```json
{
  "dimmer": 0,
  "commande": 0,
  "temperature": "45.9",
  "power": 0,
  "Ptotal": 0,
  "RSSI": -55,
  "version": "Version 20260514",
  "onoff": true,
  "alerte": "RAS",
  "dallas0": "45.9"
}
```

Le firmware privilégie `dallas0` pour la température puis utilise `temperature` en secours.

Cadence de lecture de `/state` :

```text
Dimmer en cours de synchronisation : environ 2 s
Dimmer synchronisé                 : environ 5 s
Timeout HTTP                       : 500 ms
```

---

# Régulation Zero Grid

## 13. Boucle de contrôle

Chaque nouvelle mesure Fronius validée déclenche au maximum une nouvelle décision de régulation.

Le dimmer n'est donc pas recalculé plusieurs fois à partir de la même mesure Fronius.

La logique suit principalement :

```text
P_Grid < -20 W
    -> surplus
    -> augmenter la charge ECS

-20 W <= P_Grid <= 0 W
    -> cible atteinte
    -> conserver la puissance

P_Grid > 0 W
    -> import
    -> diminuer la charge ECS
```

La cible mathématique interne est `-10 W`.

---

## 14. Limite de puissance

La charge est actuellement définie à :

```text
800 W maximum
```

Si le chauffe-eau atteint 100 % et qu'il reste encore du surplus solaire, le firmware ne peut plus l'absorber.

L'état devient alors :

```text
LOAD LIMITED
```

Le surplus restant est exporté vers le réseau.

---

## 15. Synchronisation de la consigne

Le firmware compare :

```text
consigne demandée
vs
valeur réellement remontée par /state
```

Si l'écart reste important trop longtemps, la consigne est renvoyée.

Une commande est également rafraîchie périodiquement afin de rester compatible avec l'auto-off du firmware RobotDyn.

```text
Keepalive commande : 60 s
Auto-off RobotDyn   : 5 min
```

---

# Fail-safe

## 16. Conditions qui forcent POWER=0

Le routeur demande `POWER=0` dans les cas suivants :

- Fronius inaccessible ;
- données Fronius trop anciennes ;
- `autonome=false` ;
- `onoff=false` remonté par le dimmer ;
- alarme dimmer différente de `RAS`.

En cas d'échec de la commande de sécurité, une nouvelle tentative est faite rapidement.

La régulation de sécurité ne dépend ni de Home Assistant ni du broker MQTT.

---

# MQTT / Home Assistant

## 17. Principe

MQTT est utilisé uniquement pour la **télémétrie**.

La perte de MQTT ou de Home Assistant n'arrête pas la boucle locale Fronius -> ESP32 -> RobotDyn.

Topic principal :

```text
pvrouter/state
```

Disponibilité :

```text
pvrouter/availability
```

Le firmware publie notamment :

- production PV ;
- puissance réseau ;
- consommation maison estimée ;
- puissance disponible ;
- puissance chauffe-eau ;
- consigne dimmer ;
- dimmer réel ;
- température ECS ;
- RSSI Wi-Fi ;
- état Fronius ;
- état dimmer ;
- état de synchronisation.

Home Assistant Discovery est publié automatiquement lorsque cette fonction est activée.

---

# Diagnostic

## 18. Fronius OFFLINE

Vérifier :

1. que l'ESP32 et le Fronius sont sur le même réseau ;
2. la valeur `IP_FRONIUS` ;
3. depuis un navigateur :

```text
http://<IP_FRONIUS>/solar_api/v1/GetPowerFlowRealtimeData.fcgi
```

Le fail-safe doit maintenir le dimmer à 0 % tant que les données Fronius ne sont pas valides.

---

## 19. DIMMER OFFLINE

Tester :

```bash
curl http://<IP_DIMMER>/state
```

Puis :

```bash
curl 'http://<IP_DIMMER>/?POWER=0'
```

Vérifier que l'adresse configurée dans `config.json` correspond bien au dimmer ECS.

---

## 20. Température absente

Vérifier que `/state` contient au moins l'un des champs :

```text
dallas0
temperature
```

Lorsque la communication dimmer est perdue, la température affichée est volontairement effacée afin de ne pas présenter une ancienne valeur comme actuelle.

---

## 21. Pas de routage malgré du soleil

Vérifier dans cet ordre :

1. `FRONIUS = OK` ;
2. `DIMMER = ONLINE` ;
3. `autonome=true` ;
4. aucune alarme RobotDyn ;
5. `onoff=true` ;
6. `P_Grid` devient négatif lorsque la production dépasse la consommation.

---

# Structure du projet

## 22. Fichiers principaux

```text
src/main.cpp
    création des tâches FreeRTOS

src/tasks/measure-electricity.h
    acquisition et validation Fronius

src/tasks/Dimmer.h
    watchdog local et déclenchement de la régulation

src/functions/froniusZeroGrid.h
    calcul Zero Grid et commandes RobotDyn

src/tasks/gettemp.h
    lecture /state du dimmer et température ECS

src/tasks/updateDisplay.h
    écran TTGO

src/functions/Mqtt_http_Functions.h
    MQTT et Home Assistant Discovery

src/functions/spiffsFunctions.h
    lecture / écriture config.json et wifi.json
```

L'ancien chemin Fronius redondant a été supprimé : la branche utilise maintenant une seule tâche active d'acquisition Fronius.

---

# Mise à jour OTA

## 23. OTA via l'interface Web

Lorsque le serveur Web est activé, l'OTA est accessible via :

```text
http://<IP_DU_ROUTEUR>/update
```

Construire le firmware avec PlatformIO puis envoyer le fichier `.bin` correspondant.

---

# Ancienne partie DIY / mesure locale

Le projet est issu d'un routeur PV utilisant une mesure locale par déphasage / transformateur et SCT013.

### ESP32

```text
OLED :
3.3 V
GND
21 SCL
22 SDA
```

### TTGO

```text
pin 32 GRID
pin 33 SCT013
```

La partie historique RobotDyn est issue du projet :

https://github.com/xlyric/PV-discharge-Dimmer-AC-Dimmer-KIT-Robotdyn

Une documentation historique en français est également disponible :

[Doc installation.pdf](./Doc%20installation.pdf)

---

# Développement

## 24. CI GitHub

Le workflow :

```text
.github/workflows/platformio-v13-dimmer.yml
```

compile automatiquement l'environnement :

```text
ttgo-t-display
```

Cela permet de détecter les erreurs de compilation avant le flash matériel.

---

## 25. Philosophie de la branche V13

Les priorités sont :

1. récupérer une seule fois chaque mesure Fronius ;
2. valider les données avant de les utiliser ;
3. réguler localement sans dépendance au cloud ;
4. utiliser l'état réel du dimmer ;
5. revenir à `POWER=0` en cas de doute ;
6. garder MQTT / Home Assistant uniquement pour l'observation ;
7. conserver une régulation simple et explicable.

---

## 26. État actuel

Fonctions validées sur matériel :

- connexion Fronius ;
- lecture `P_Grid` / `P_PV` ;
- communication RobotDyn `/state` ;
- lecture température Dallas du chauffe-eau ;
- affichage TTGO ;
- commandes absolues `POWER=0..100` ;
- fail-safe local ;
- MQTT / Home Assistant Discovery.

La validation finale du comportement dynamique Zero Grid doit être réalisée en conditions de production solaire afin d'observer les transitoires lors des changements rapides de production et de consommation.

---

## Licence

Voir [LICENSE](./LICENSE).
