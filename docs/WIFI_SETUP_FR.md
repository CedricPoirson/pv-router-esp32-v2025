# Première configuration Wi‑Fi — PV Router V14.6

Cette procédure concerne le **TTGO T‑Display** utilisé par cette branche du PV Router.

Le mode de configuration Wi‑Fi n'est **jamais lancé automatiquement** en cas de panne de box ou de réseau. Il faut une action physique volontaire sur le bouton du TTGO au démarrage.

## Entrer en mode configuration

1. Couper l'alimentation du PV Router.
2. Maintenir le bouton utilisateur du TTGO (`GPIO35`).
3. Remettre l'alimentation tout en gardant le bouton appuyé.
4. Maintenir environ **3 secondes** jusqu'à l'apparition de `MODE CONFIG WIFI`.
5. Relâcher le bouton.

Le TTGO affiche alors :

```text
MODE CONFIG WIFI
Wi-Fi : PVRouter-Setup
Mot de passe : pvrouter14
Puis ouvrir :
http://192.168.4.1
```

## Se connecter avec un téléphone ou un ordinateur

Se connecter au point d'accès :

```text
SSID : PVRouter-Setup
Mot de passe : pvrouter14
```

Le téléphone peut proposer automatiquement d'ouvrir la page captive. Sinon ouvrir manuellement :

```text
http://192.168.4.1
```

La page affiche un formulaire bilingue FR/EN et une liste des réseaux Wi‑Fi détectés.

Saisir :

- le SSID du Wi‑Fi de la maison ;
- le mot de passe Wi‑Fi.

Puis cliquer **Enregistrer et redémarrer**.

Les identifiants sont enregistrés dans :

```text
/wifi.json
```

sur SPIFFS. Ils prennent ensuite **priorité sur les identifiants compilés dans `config.h`**.

Le PV Router redémarre automatiquement, coupe son point d'accès `PVRouter-Setup` et tente de rejoindre le nouveau réseau.

## Démarrage normal

Si le bouton n'est pas maintenu au démarrage, le PV Router fonctionne normalement.

Ordre de choix des identifiants Wi‑Fi :

```text
1. /wifi.json si un vrai SSID y est enregistré
2. WIFI_NETWORK / WIFI_PASSWORD de src/config/config.h en secours
```

La valeur historique `xxx` signifie simplement « aucune configuration Wi‑Fi SPIFFS valide ».

## Si le Wi‑Fi de la maison est indisponible

Le firmware attend jusqu'à `WIFI_TIMEOUT` (20 s dans la configuration de référence), puis poursuit son démarrage en mode Wi‑Fi hors ligne et relance périodiquement les tentatives de connexion en arrière-plan.

Le TTGO affiche notamment :

```text
WI-FI INDISPONIBLE
Boot + bouton 3 s = config
```

Le point d'accès de configuration **n'est pas lancé automatiquement**. Cela évite qu'une simple panne de box expose un réseau de maintenance.

Pour changer de réseau Wi‑Fi, redémarrer volontairement le PV Router avec le bouton maintenu 3 secondes.

## Sécurité pendant le mode configuration

Le mode configuration est un mode de maintenance dédié : les tâches normales de régulation ne sont pas démarrées tant que le formulaire n'a pas été sauvegardé et que le PV Router n'a pas redémarré.

Le point d'accès utilise WPA2 avec le mot de passe `pvrouter14` et n'existe que lorsqu'il est demandé physiquement au boot.

## SPIFFS et `uploadfs`

`pio run -t uploadfs` remplace le filesystem SPIFFS. Il peut donc écraser `/wifi.json`.

Avant un `uploadfs`, conserver une copie du Wi‑Fi si nécessaire ou prévoir de refaire la procédure physique de configuration après le flash.

Le fichier modèle reste :

```text
data/wifi.json.ori
```

mais il n'est plus nécessaire d'éditer ce fichier manuellement pour changer de réseau : le portail V14.6 permet de le faire directement depuis un téléphone.

## Bouton utilisé

Le portail utilise le **même bouton GPIO35** que la navigation de l'écran en fonctionnement normal.

- au démarrage : maintien 3 s → configuration Wi‑Fi ;
- en fonctionnement : appui court → page suivante ;
- en fonctionnement : appui long → écran OFF.

Aucun second bouton n'est requis pour cette fonction.
