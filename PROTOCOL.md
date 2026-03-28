# Reverse-engineering du protocole BLE On.e (ACIS)

## Sommaire

- [Informations générales](#informations-générales)
- [UUIDs BLE](#uuids-ble)
- [Flux de connexion](#flux-de-connexion)
- [Authentification (AES-128-ECB)](#authentification-aes-128-ecb)
- [Commandes (pompe / lumière)](#commandes-pompe--lumière)
- [Notifications de statut](#notifications-de-statut)
- [Mapping Handles - UUIDs](#mapping-handles--uuids)
- [Ce qui manque](#ce-qui-manque)
- [Prochaines étapes](#prochaines-étapes)

---

## Informations générales

| Champ | Valeur |
|-------|--------|
| Fabricant | ACIS (marque On.e) |
| App Android | `com.acis.oneapp` v2.6.3 (React Native + Hermes) |
| Lib BLE | `react-native-ble-plx` |
| Crypto JS | `aes-js` (AES-128-ECB) |
| Puce BLE | Silicon Labs (préfixe MAC `1C:34:F1`) |
| Adresse MAC testée | *(propre à chaque device)* |
| Nom BLE annoncé | `ON.E` |
| Numéro de série | `23240` |
| Version firmware | `1.02` |
| Modèle | `ONE` (contrôleur pompe + éclairage basique) |

---

## UUIDs BLE

Tous les UUIDs customs partagent la base : `FBDE????-4C7B-4E67-8292-A9B8E686CF87`

Les services standards utilisent les UUIDs Bluetooth SIG classiques.

### Service Système (Group 00) — Authentification

| UUID complet | Nom | Rôle |
|------|-----|------|
| `FBDE0000-4C7B-4E67-8292-A9B8E686CF87` | `SERVICE_SYSTEM_UUID` | Service système (aussi `ADVERTISING_SERVICE_UUID_USE`) |
| `FBDE0001-4C7B-4E67-8292-A9B8E686CF87` | `SERVICE_SYSTEM_RANDOMKEY_UUID` | **Nonce** (lecture, 8 octets, change à chaque connexion) |
| `FBDE0002-4C7B-4E67-8292-A9B8E686CF87` | `SERVICE_SYSTEM_SHAREDKEY_UUID` | **Shared key** (lecture lors de l'association initiale) |
| `FBDE0003-4C7B-4E67-8292-A9B8E686CF87` | `SERVICE_SYSTEM_ENCRYPTKEY_UUID` | **Réponse auth chiffrée** (écriture, 16 octets) |

### Services standards — Identification et horloge

| UUID | Nom | Rôle |
|------|-----|------|
| `180A` | Device Information Service | Service info device |
| `2A24` | `SERVICE_DEVICE_MODELNUMBER_UUID` | Numéro de modèle |
| `2A25` | `SERVICE_DEVICE_SERIALNUMBER_UUID` | Numéro de série |
| `2A26` | `SERVICE_DEVICE_FIRMWAREVERSION_UUID` | Version firmware |
| `1805` | Current Time Service | Service horloge |
| `2A08` | `SERVICE_TIME_DATETIME_UUID` | Date/heure (synchro RTC) |
| `2A09` | `SERVICE_TIME_DAY_UUID` | Jour |

### Service Advertising — Association

| UUID complet | Nom | Rôle |
|------|-----|------|
| `FBDE0100-4C7B-4E67-8292-A9B8E686CF87` | `ADVERTISING_SERVICE_UUID_ASSOCIATION` | UUID annoncé en mode appairage |
| `FBDE0000-4C7B-4E67-8292-A9B8E686CF87` | `ADVERTISING_SERVICE_UUID_USE` | UUID annoncé en mode normal |

### Service One (Group 02) — Contrôle pompe + éclairage

| UUID complet | Nom | Rôle |
|------|-----|------|
| `FBDE0200-4C7B-4E67-8292-A9B8E686CF87` | `SERVICE_ONE_UUID` | Service principal |
| `FBDE0201-4C7B-4E67-8292-A9B8E686CF87` | `SERVICE_ONE_STATUS_UUID` | **Statut** (lecture + notifications) |
| `FBDE0202-4C7B-4E67-8292-A9B8E686CF87` | `SERVICE_ONE_CONTROLE_UUID` | **Commande** (écriture) |
| `FBDE0203-4C7B-4E67-8292-A9B8E686CF87` | `SERVICE_ONE_PARAMETRAGE_FILTRATION_UUID` | Paramétrage filtration/éclairage |

### Service OTA — Mise à jour firmware

| UUID complet | Nom |
|------|-----|
| `1D14D6EE-FD63-4FA1-BFA4-8F47B42119F0` | `SERVICE_OTA` |
| `F7BF3564-FB6D-4E53-88A4-5E37E0326063` | `SERVICE_OTA_CONTROL` |
| `984227F3-34FC-4045-A5D0-2C581F81A153` | `SERVICE_OTA_DATA` |

### Autres services (non utilisés pour le modèle ONE basique)

| Group | UUID base | Modèle |
|-------|-----------|--------|
| 03 | `FBDE03xx` | OneVS (pompe vitesse variable) |
| 04 | `FBDE04xx` | OneVS 2K2 |
| 05 | `FBDE05xx` | TLC3 One (contrôleur éclairage avancé) |
| 06 | `FBDE06xx` | OneSalt (électrolyseur sel) |
| 07 | `FBDE07xx` | OneConstance (niveau d'eau) |
| 08 | `FBDE08xx` | OneConnect (passerelle WiFi/MQTT) |
| 09 | `FBDE09xx` | OneLibra / OneRevolusel (dosage chimique) |

---

## Flux de connexion

L'app suit 7 phases séquentielles :

```
1. connectProcess        -> Connexion BLE, MTU 512, découverte services
2. associationProcess    -> Lecture shared key (uniquement au 1er appairage)
3. authorisationProcess  -> Challenge-response AES-128-ECB
4. identificationProcess -> Lecture modèle, série, firmware
5. syncRTCProcess        -> Synchronisation horloge temps réel
6. utilisationProcess    -> Mode utilisation normal
7. disconnectProcess     -> Déconnexion propre
```

### Séquence observée dans le log HCI (session 2)

```
1. Connexion BLE
2. Échange MTU (247 demandé)
3. Découverte services et caractéristiques
4. Lecture FBDE0002 (sharedKey) — ou FBDE0001 (randomKey/nonce)
5. Lecture FBDE0001 (randomKey/nonce)
6. Écriture FBDE0003 (réponse auth chiffrée, 16 octets)
7. Écriture 2A08 (synchro date/heure)
8. Écriture config byte 0x06
9. Activation notifications sur FBDE0201 (écriture CCCD = 0x0100)
10. Lecture FBDE0201 (statut initial)
11. Écriture FBDE0202 (commandes pompe/lumière)
```

---

## Authentification (AES-128-ECB)

**L'algorithme a été entièrement reverse-engineeré** depuis le bytecode Hermes v96 via `hermes-dec`.

**Il n'y a PAS de pairing SMP classique.** La connexion BLE est non chiffrée au niveau link-layer. L'authentification est **propriétaire**, au niveau applicatif.

### Clé AES hardcodée (identique pour tous les devices)

```
PRIVATE_KEY = "1141a80537444a6a85888d84115f2811"

En octets : [0x11, 0x41, 0xA8, 0x05, 0x37, 0x44, 0x4A, 0x6A,
             0x85, 0x88, 0x8D, 0x84, 0x11, 0x5F, 0x28, 0x11]
```

### Phase 1 : Association (premier appairage uniquement)

L'association ne se fait qu'une seule fois, lors du premier appairage avec le device. La `sharedKey` obtenue est ensuite stockée de manière persistante.

```
1. Lire la caractéristique FBDE0002 (SERVICE_SYSTEM_SHAREDKEY_UUID)
   du service FBDE0000 (SERVICE_SYSTEM_UUID)
2. Décoder : rawBytes = base64Decode(characteristic.value)
3. Inverser l'ordre des octets (little-endian BLE -> big-endian)
4. Convertir en chaîne hex : sharedKey = bytesToHex(reverse(rawBytes))
5. Stocker sharedKey (persistant, par device)
```

La sharedKey fait 8 octets (16 caractères hex), ex : `"0123456789abcdef"`.

### Phase 2 : Authentification (à chaque connexion)

```python
def authorisationProcess(device):
    PRIVATE_KEY = "1141a80537444a6a85888d84115f2811"

    # 1. Lire le nonce depuis le device
    nonce_raw = base64_decode(
        read_characteristic(
            service  = "FBDE0000-4C7B-4E67-8292-A9B8E686CF87",
            charact  = "FBDE0001-4C7B-4E67-8292-A9B8E686CF87"
        ).value
    )

    # 2. Inverser les octets du nonce (little-endian BLE -> big-endian)
    nonce_hex = bytes_to_hex(reverse(nonce_raw))

    # 3. Récupérer la shared key stockée
    shared_key_hex = device.sharedKey  # stockée depuis l'association

    # 4. Construire le plaintext : sharedKey || nonce (concaténation hex)
    plaintext_hex = shared_key_hex + nonce_hex
    # -> exactement 32 chars hex = 16 octets = 1 bloc AES

    # 5. Chiffrer avec AES-128-ECB
    key_bytes = hex_to_bytes(PRIVATE_KEY)
    plaintext_bytes = hex_to_bytes(plaintext_hex)
    encrypted_bytes = AES_ECB(key_bytes).encrypt(plaintext_bytes)

    # 6. Inverser le résultat (big-endian -> little-endian BLE)
    response = reverse(encrypted_bytes)

    # 7. Écrire la réponse sur le device
    write_characteristic(
        service  = "FBDE0000-4C7B-4E67-8292-A9B8E686CF87",
        charact  = "FBDE0003-4C7B-4E67-8292-A9B8E686CF87",
        value    = base64_encode(response)
    )
```

### Résumé visuel

```
                        BLE (little-endian)
                              |
    Lecture FBDE0001 -------> | nonce_raw (8 octets)
                              |
                         reverse()
                              |
                        nonce_hex (big-endian)
                              |
    sharedKey (stocké) -----> | + nonce_hex
                              |
                        plaintext (16 octets = 1 bloc AES)
                              |
                    AES-128-ECB(PRIVATE_KEY)
                              |
                        encrypted (16 octets)
                              |
                         reverse()
                              |
    Écriture FBDE0003 <------ | response (little-endian BLE)
```

### Points importants

- **Pas de `deviceSalt`** dans le crypto : cette chaîne dans l'APK fait référence à une image PNG (icône UI), pas à un sel cryptographique.
- Le nonce fait **8 octets** (pas 5 comme vu dans le log HCI tronqué).
- `sharedKey` (8 octets) + `nonce` (8 octets) = 16 octets = exactement 1 bloc AES-128.
- Toutes les données BLE sont **inversées** (little-endian sur le fil, big-endian dans l'app).
- L'écriture de la réponse utilise `writeCharacteristicWithResponseForService` (write with response).

### Données observées dans le log HCI

| Session | Nonce (Handle 0x0015, tronqué) | Réponse auth (Handle 0x0019, tronqué) |
|---------|-------------------------------|---------------------------------------|
| 1 | `07 DC E7 F1 BC` (5 premiers octets visibles sur 8) | `99 D4 69 ...` (3 premiers octets sur 16) |
| 2 | `63 6C 13 1F 7C` (5 premiers octets visibles sur 8) | `C4 56 E5 ...` (3 premiers octets sur 16) |

---

## Commandes (pompe / lumière)

### Caractéristique de commande

- **UUID** : `FBDE0202-4C7B-4E67-8292-A9B8E686CF87` (`SERVICE_ONE_CONTROLE_UUID`)
- **Handle** : `0x0028`
- **Type d'écriture** : Write Without Response
- **Taille** : 1 octet (bitmask)

### Table de commandes

| Octet | Binaire | Pompe | Lumiere | Description |
|-------|---------|-------|---------|-------------|
| `0x00` | `0000 0000` | OFF | OFF | Tout éteint |
| `0x01` | `0000 0001` | **ON** | OFF | Pompe seule |
| `0x04` | `0000 0100` | OFF | **ON** | Lumiere seule |
| `0x05` | `0000 0101` | **ON** | **ON** | Pompe + Lumiere |

### Mapping des bits

| Bit | Masque | Fonction |
|-----|--------|----------|
| Bit 0 | `0x01` | Relais pompe |
| Bit 2 | `0x04` | Relais lumiere |

> **Important** : L'app envoie toujours l'**etat complet souhaite**, pas un toggle individuel. Pour allumer la lumiere sans toucher a la pompe (deja en marche), il faut envoyer `0x05` (pas `0x04`).

### Sequence complete observee

```
Temps    Commande    Statut retourne    Action
-----    --------    ---------------    ------
+0.0s    (connexion + auth)
+6.7s    0x01        0x05               Pompe ON
+11.5s   0x00        0x00               Pompe OFF
+14.9s   0x04        0x28               Lumiere ON
+19.1s   0x00        0x00               Lumiere OFF
+22.2s   0x01        0x05               Pompe ON
+24.2s   0x05        0x2D               Pompe + Lumiere ON
+27.5s   0x04        0x28               Pompe OFF (lumiere reste)
+29.3s   0x00        0x00               Tout OFF
```

---

## Notifications de statut

### Caracteristique de statut

- **UUID** : `FBDE0201-4C7B-4E67-8292-A9B8E686CF87` (`SERVICE_ONE_STATUS_UUID`)
- **Handle** : `0x0030` (CCCD sur `0x0031`)
- **Mode** : Notifications (ecrire `0x01 0x00` sur CCCD)
- **Frequence** : ~1 notification/seconde
- **Taille** : 1 octet

### Table des statuts

| Octet | Signification |
|-------|---------------|
| `0x00` | Tout eteint |
| `0x02` | Connecte, pret (etat initial) |
| `0x05` | Pompe active |
| `0x28` | Lumiere active |
| `0x2D` | Pompe + Lumiere actives |

> **Note** : Le mapping des bits du statut est **different** de celui des commandes. Le statut contient probablement d'autres informations (mode, alarmes, etc.).

---

## Mapping Handles - UUIDs

Base sur la correlation entre le log HCI et les UUIDs de l'APK :

| Handle | UUID | Rôle |
|--------|------|------|
| 0x0015 | `FBDE0001` (RANDOMKEY) | Nonce (8 octets, tronque a 5 dans la capture) |
| 0x0017 | `FBDE0002` (SHAREDKEY) | Shared key (lecture lors de l'association) |
| 0x0019 | `FBDE0003` (ENCRYPTKEY) | Reponse auth chiffree (ecriture, 16 octets) |
| 0x001C | `2A08` (DATETIME) | Synchro date/heure |
| 0x001E | `2A09` (DAY) | Jour de la semaine |
| 0x0021 | GAP Device Name | Nom : "ON.E" |
| 0x0023 | `2A25` (SERIAL) | Numero de serie : "23240" |
| 0x0025 | `2A26` (FIRMWARE) | Version firmware : "1.02" |
| 0x0028 | **`FBDE0202` (CONTROLE)** | **Commande pompe/lumiere** |
| 0x0030 | **`FBDE0201` (STATUS)** | **Statut (notifications)** |
| 0x0031 | CCCD de 0x0030 | Activation notifications |

---

## Ce qui manque

### Resolu (grace a la decompilation Hermes)

- ~~Algorithme d'authentification~~ -> **RESOLU** : AES-128-ECB avec cle hardcodee `1141a80537444a6a85888d84115f2811`
- ~~Derivation de cle~~ -> **RESOLU** : plaintext = sharedKey (8 octets) + nonce (8 octets), chiffre avec PRIVATE_KEY
- ~~deviceSalt~~ -> **RESOLU** : n'existe pas dans le crypto, c'est une image UI

### Reste a faire / valider

1. **Obtenir la sharedKey de votre device** — La sharedKey est echangee lors du tout premier appairage via FBDE0002. Il faut soit :
   - Extraire la sharedKey stockee dans les donnees de l'app Android (SharedPreferences ou AsyncStorage de React Native)
   - Refaire un appairage et capturer la lecture de FBDE0002

2. **Valider le nonce de 8 octets** — Le log HCI tronque les paquets a ~5 octets. Confirmer avec nRF Connect que le nonce fait bien 8 octets.

3. **Signification complete de l'octet de statut** — Les bits exacts du statut (`0x02`, `0x05`, `0x28`, `0x2D`) ne correspondent pas directement au bitmask des commandes. La structure complete (mode, alarmes, etc.) est inconnue.

4. **Format exact de la synchro RTC** — L'ecriture sur `2A08` suit probablement le format standard Bluetooth Current Time, mais a confirmer.

5. **Caracteristique de parametrage** (`FBDE0203`) — Programmation horaire de la filtration/eclairage. Non exploree dans la capture.

---

## Prochaines etapes

### 1. Recuperer la sharedKey

**Option A** : Extraire depuis le stockage de l'app Android
```bash
# Sur un Android roote (ou via adb backup)
# L'app React Native stocke ses donnees dans AsyncStorage
adb shell run-as com.acis.oneapp cat /data/data/com.acis.oneapp/databases/RKStorage
# ou
adb shell run-as com.acis.oneapp cat /data/data/com.acis.oneapp/files/AsyncStorage/*
```

**Option B** : Lire FBDE0002 avec nRF Connect lors d'un nouvel appairage
- Mettre le device en mode appairage
- Se connecter avec nRF Connect
- Lire FBDE0002 du service FBDE0000
- Inverser les octets et convertir en hex

### 2. Tester l'authentification

Une fois la sharedKey obtenue, tester avec le script Python (`analysis/auth-implementation.py`) en comparant avec les nonces/reponses du log HCI.

### 3. Implementer le firmware ESP32 ESPHome

```yaml
esphome:
  name: one-pool-controller

esp32:
  board: esp32dev

# Le composant custom BLE devra :
# 1. Scanner pour "ON.E" avec UUID FBDE0000
# 2. Se connecter
# 3. Lire FBDE0001 (nonce, 8 octets)
# 4. Calculer AES-128-ECB(PRIVATE_KEY, reverse(sharedKey) + reverse(nonce))
# 5. Ecrire reverse(encrypted) sur FBDE0003
# 6. Synchro RTC sur 2A08
# 7. Activer notifications sur FBDE0201
# 8. Commander via FBDE0202
```

---

## Annexe A — Noms BLE des devices On.e

| Nom annonce | Modele |
|-------------|--------|
| `ON.E` / `On.e` | ONE (pompe basique) |
| `CL ON.E` / `CLON.E` | CLONE |
| `ON.EVS` / `On.e VS` | ONEVS (vitesse variable) |
| `ON.EVS2K2` | ONEVS2K2 |
| `TLC3ON.E` / `TLC3 On.e` | TLC3ONE (eclairage) |
| `ON.E CONNECT` | ONECONNECT (passerelle WiFi) |
| `CONSTANCE` | ONECONSTANCE (niveau d'eau) |
| `LIBRA` | ONELIBRA (dosage) |
| `REVOLUSEL` | ONEREVOLUSEL (sel) |

## Annexe B — Source de la decompilation

L'algorithme d'authentification a ete extrait par decompilation du bytecode Hermes v96 avec `hermes-dec`. Le code decompile brut est disponible dans `analysis/decompiled-auth-functions.txt`. Les fonctions cles sont :

- **Function #14327/#14328** : `associationProcess` (lecture sharedKey)
- **Function #14331/#14334/#14335** : `authorisationProcess` (challenge-response AES)
- **Function #14340/#14343/#14344** : `identificationProcess` (lecture infos device)
- **Initialisation des constantes** : UUIDs et `PRIVATE_KEY` (ligne 42 du code decompile)
