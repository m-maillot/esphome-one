# On.e Pool BLE Bridge for ESPHome / Home Assistant

Pont BLE-WiFi pour piloter un module **On.e** (ACIS) de piscine depuis **Home Assistant**, via un **ESP32** et **ESPHome**.

Le module On.e gere l'allumage/extinction de la pompe et de l'eclairage de piscine, mais n'est accessible qu'en Bluetooth (BLE). Ce projet expose ces controles en WiFi grace a un ESP32 qui fait office de passerelle.

## Fonctionnalites

- Allumage / extinction de la **pompe**
- Allumage / extinction de l'**eclairage**
- Retour d'etat en temps reel (notifications BLE)
- Integration native Home Assistant via ESPHome
- Authentification BLE propriétaire entierement implementee (AES-128-ECB)
- Synchronisation automatique de l'horloge du module

## Materiel necessaire

- Un module **On.e** (ACIS) installe dans le tableau electrique
- Une carte **ESP32** (ESP32-DevKit, ESP32-WROOM, etc.)
- Un acces WiFi a portee de l'ESP32

## Structure du projet

```
.
├── README.md                  # Ce fichier
├── PROTOCOL.md                # Documentation complete du protocole BLE
├── docs/
│   ├── index.html             # Page de presentation (GitHub Pages)
│   └── test-ble.html          # Page de test Web Bluetooth (association + controle)
├── esphome/
│   ├── one-pool.yaml          # Configuration ESPHome principale
│   ├── secrets.yaml           # Parametres propres a votre installation
│   └── components/
│       └── one_pool/
│           ├── __init__.py    # Composant ESPHome (Python)
│           ├── switch.py      # Plateforme switch
│           ├── binary_sensor.py # Plateforme binary_sensor
│           ├── sensor.py      # Plateforme sensor (diagnostic)
│           ├── one_pool.h     # Code C++ (header)
│           └── one_pool.cpp   # Code C++ (implementation BLE + AES)
└── analysis/                  # Fichiers de reverse-engineering
    ├── auth-algorithm.txt     # Algorithme d'authentification detaille
    ├── auth-implementation.py # Implementation Python de l'auth
    ├── all-fbde-uuids.txt     # Liste de tous les UUIDs decouverts
    └── decompiled-auth-functions.txt  # Code decompile de l'APK
```

## Installation

### 1. Recuperer la SharedKey de votre device

Chaque module On.e possede une **SharedKey** unique echangee lors du premier appairage. Vous devez la recuperer une seule fois.

Ouvrez l'**[outil BLE en ligne](https://m-maillot.github.io/esphome-one/test-ble.html)** sur Chrome Android :

1. Mettez le module On.e en **mode appairage** (bouton physique sur le module)
2. Cliquez sur **Scanner** et selectionnez votre device
3. La SharedKey s'affiche automatiquement — copiez-la
4. Notez aussi l'**adresse MAC** du device (visible lors du scan)

### 2. Configurer ESPHome

Editez `esphome/secrets.yaml` avec vos valeurs :

```yaml
# WiFi
wifi_ssid: "MonReseau"
wifi_password: "MonMotDePasse"

# ESPHome
api_key: "generez-une-cle-avec-esphome"
ota_password: "un-mot-de-passe-ota"

# Device On.e
one_mac_address: "XX:XX:XX:XX:XX:XX"   # Adresse MAC de votre module
one_shared_key: "0123456789abcdef"       # SharedKey recuperee a l'etape 1
```

### 3. Flasher l'ESP32

```bash
# Depuis le dossier esphome/
esphome run one-pool.yaml
```

Ou via le dashboard ESPHome de Home Assistant :
1. Copiez le dossier `esphome/` dans votre configuration ESPHome
2. Adoptez le device dans le dashboard
3. Flashez

### 4. Ajouter a Home Assistant

Le device apparait automatiquement dans Home Assistant avec :

#### Controles

| Entite | Type | Description |
|--------|------|-------------|
| `switch.pompe_piscine` | Switch | Allumer / eteindre la pompe |
| `switch.lumiere_piscine` | Switch | Allumer / eteindre l'eclairage |

#### Statuts

| Entite | Type | Description |
|--------|------|-------------|
| `binary_sensor.pompe_piscine_statut` | Binary Sensor | Etat reel de la pompe (via notification BLE) |
| `binary_sensor.lumiere_piscine_statut` | Binary Sensor | Etat reel de l'eclairage (via notification BLE) |

#### Diagnostic

| Entite | Type | Description |
|--------|------|-------------|
| `binary_sensor.ble_connecte` | Binary Sensor (connectivity) | ON = ESP32 connecte et authentifie au module On.e |
| `sensor.deconnexions_ble` | Sensor (total_increasing) | Nombre de deconnexions BLE depuis le dernier reboot |
| `sensor.wifi_signal` | Sensor (dBm) | Force du signal WiFi de l'ESP32 |
| `sensor.uptime` | Sensor (s) | Temps depuis le dernier redemarrage de l'ESP32 |

#### Exemple d'automatisation : alerte perte de connexion

```yaml
automation:
  - alias: "Alerte perte connexion BLE piscine"
    trigger:
      - platform: state
        entity_id: binary_sensor.ble_connecte
        to: "off"
        for: "00:05:00"
    action:
      - service: notify.mobile_app_votre_telephone
        data:
          title: "⚠️ Piscine"
          message: "Connexion BLE perdue depuis 5 min. La pompe ne peut plus etre pilotee !"
```

> **Astuce placement** : utilisez le capteur `sensor.deconnexions_ble` pour trouver le meilleur emplacement de l'ESP32. Si le compteur monte rapidement, l'ESP32 est trop loin du module On.e.

## Modeles supportes

Ce projet a ete teste avec le modele **ONE** (controle pompe + eclairage basique). D'autres modeles On.e existent avec des services BLE supplementaires :

| Modele | Nom BLE | Service | Statut |
|--------|---------|---------|--------|
| ONE | `ON.E` | FBDE02xx | Supporte |
| ONE VS | `ON.EVS` | FBDE03xx | Non teste |
| TLC3 ONE | `TLC3ON.E` | FBDE05xx | Non teste |
| ONE Salt | `ON.E SALT` | FBDE06xx | Non teste |
| ONE Connect | `ON.E CONNECT` | FBDE08xx | Non teste |

## Protocole BLE

Le protocole est entierement documente dans [PROTOCOL.md](PROTOCOL.md). En resume :

1. **Connexion** BLE au device (UUID `FBDE0000`)
2. **Authentification** challenge-response AES-128-ECB avec cle hardcodee
3. **Synchronisation** de l'horloge RTC
4. **Controle** via ecriture d'un octet sur la caracteristique de commande
5. **Statut** via notifications BLE

## Depannage

### L'ESP32 ne se connecte pas au module

- Verifiez que l'adresse MAC dans `secrets.yaml` est correcte
- Assurez-vous que l'ESP32 est a portee BLE du module (< 10m en general)
- Verifiez que le module On.e n'est pas connecte a un telephone (une seule connexion BLE a la fois)
- Consultez les logs ESPHome (`esphome logs one-pool.yaml`)

### L'authentification echoue

- Verifiez que la SharedKey est correcte (16 caracteres hexadecimaux)
- Refaites la procedure d'association avec `test-ble.html` si necessaire

### Les commandes ne fonctionnent pas

- Verifiez dans les logs que l'etat est `=== READY ===`
- Le module peut mettre quelques secondes a repondre apres l'authentification

## Comment ca a ete reverse-engineere

1. **Decompilation de l'APK** Android (`com.acis.oneapp`) — app React Native avec bytecode Hermes, decompilee via `hermes-dec`
2. **Capture HCI** Bluetooth sur Android pour observer les echanges reels
3. **Page de test Web Bluetooth** (`test-ble.html`) pour valider interactivement chaque etape du protocole

Les details complets sont dans le dossier `analysis/` et dans [PROTOCOL.md](PROTOCOL.md).

## Licence

Ce projet est un travail de reverse-engineering a des fins d'interoperabilite personnelle. Il n'est pas affilie a ACIS ni a la marque On.e.

## Contribuer

Les contributions sont bienvenues, notamment pour :
- Supporter d'autres modeles On.e (VS, TLC3, Salt, etc.)
- Ameliorer la robustesse de la connexion BLE
- Ajouter le parametrage de la filtration (caracteristique FBDE0203)
