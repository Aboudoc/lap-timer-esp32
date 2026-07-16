# Lap Timer GPS — ESP32 🏍️⏱️

Chronomètre de tours GPS autonome, fait maison, pour les track days moto
(développé pour une Ninja 400 au circuit MSP Bangkok). Monté sur le réservoir
via Quad Lock, il détecte tout seul chaque passage sur la ligne de
départ/arrivée et affiche :

- le **temps du tour en cours** en gros chiffres,
- à chaque tour bouclé : le **temps du tour** plein écran pendant 4 s avec
  l'**écart vs ton meilleur tour** (`-0.32 vs meilleur`),
- le dernier tour, le meilleur tour, la vitesse, le nombre de tours,
- et il **enregistre tous les tours en CSV** dans sa mémoire (récupérables par USB).

La ligne de départ se définit une seule fois, en roulant, d'un appui sur un
bouton — elle est mémorisée pour toutes les sorties suivantes. Précision
réelle : ~0,1-0,3 s (GPS 5 Hz + interpolation du franchissement).

---

## 1. Les pièces et leur rôle

### Électronique

| Pièce | Rôle | Prix approx. |
|---|---|---|
| **ESP32 DevKit** (CH340, USB-C) | Le cerveau. Lit le GPS, calcule les temps, pilote l'écran. Se programme par USB. | ~160 ฿ |
| **GPS GY-NEO-6M** + antenne | Donne position, vitesse, cap et heure précise, 5 fois par seconde après configuration. | ~110 ฿ |
| **Écran OLED 0,96" SSD1306 (I2C)** | Affichage. Très contrasté, lisible au soleil, ne consomme presque rien. | ~85 ฿ |
| **TP4056** (micro-USB) | Chargeur de la batterie LiPo : tu recharges l'appareil comme un téléphone. | ~15 ฿ |
| **Batterie LiPo 3,7 V ≥1000 mAh** | Alimente le tout. 1500 mAh ≈ 6-8 h d'autonomie, large pour une journée. | ~150 ฿ |
| **Boost MT3608** | Élève le 3,7 V de la batterie en 5 V stable. Sans lui, l'ESP32 + GPS planteraient (tension trop juste). | ~20 ฿ |
| **Interrupteur** à glissière | Marche/arrêt entre la batterie et le reste. | ~10 ฿ |
| **Bouton poussoir étanche 12 mm** *(optionnel)* | Changer de page / définir la ligne avec des gants. Le bouton BOOT de l'ESP32 fait le même travail pour débuter. | ~25 ฿ |
| **Fils Dupont** + **boîtier ABS** | Câblage de prototypage et coque de protection. | ~50 ฿ |

> ⚠️ **Fils Dupont** : pour relier deux modules à pins mâles (ESP32 ↔ OLED/GPS),
> il faut du **femelle-femelle**. Le mâle-femelle sert avec une breadboard.
> Vérifie ce que tu as ; un lot F-F coûte ~20 ฿.

> 💡 **Pas encore de batterie ?** Une petite power bank sur le port USB-C de
> l'ESP32 remplace toute la chaîne batterie/TP4056/MT3608 — zéro soudure
> d'alimentation pour commencer.

### Outils

| Outil | Pour quoi faire |
|---|---|
| Fer à souder + étain | Souder les pins du GPS (livrées en vrac), les fils batterie, le montage final anti-vibrations |
| Multimètre | **Obligatoire** : régler le MT3608 à 5 V avant de brancher l'ESP32 (il sort >20 V d'usine !) |
| Colle chaude, mousse 3M, gaine thermo | Fixer les connecteurs et l'antenne contre les vibrations de la moto |
| Adaptateur universel Quad Lock | Se colle au dos du boîtier → l'appareil se clipse sur le support du réservoir |

---

## 2. Comment ça marche (le principe)

Le GPS envoie une position 5 fois par seconde. Le firmware trace une **porte
virtuelle** de 40 m de large, centrée sur la ligne de départ/arrivée,
perpendiculaire au sens de passage :

```
                       │◄────── 40 m ──────►│
      ─ ─ ─ ─ ─ ─ ─ ─ ─┼────────────────────┼─ ─ ─ ─ ─ ─
                            ▲    ▲
                       t₁ ● │    │           t₁, t₂ : deux fixes GPS successifs (0,2 s)
                            │    │           Le franchissement exact est interpolé
                       t₂ ● │    │           entre les deux → précision ≈ 0,1 s
                          sens de passage
```

Quand le segment entre deux positions successives coupe la porte **dans le bon
sens**, un tour est compté. Le moment exact du franchissement est interpolé
entre les deux fixes (avec l'heure GPS, précise à la milliseconde) — c'est ce
qui donne une précision bien meilleure que « 1 fix toutes les 0,2 s ».

Garde-fous intégrés : temps au tour minimum 30 s, vitesse minimum 15 km/h,
passage dans le mauvais sens ignoré → pas de faux tours dans les stands ou à
l'arrêt sur la grille.

---

## 3. Assemblage, étape par étape

Chaque étape a un point de contrôle ✓ : ne passe à la suivante que s'il est bon.

### Étape 0 — Installer l'environnement (sans rien câbler)

1. Installe [VS Code](https://code.visualstudio.com/) puis l'extension
   **PlatformIO IDE** (ou juste le CLI : `pip install platformio`).
2. Clone ce repo et ouvre le dossier.
3. Branche l'ESP32 en USB.

✓ *Contrôle :* `pio device list` (ou l'icône PlatformIO) montre un port
`/dev/cu.usbserial-*` ou `/dev/cu.wchusbserial-*` (macOS ; le pilote CH340 est
intégré aux macOS récents).

### Étape 1 — L'écran, en mode simulation

Câble l'OLED sur l'ESP32 (4 fils, ESP32 **éteint**) :

| OLED | ESP32 |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO21 |
| SCL | GPIO22 |

Flashe le **mode simulation** — l'appareil roule sur un circuit virtuel, sans
GPS :

```bash
pio run -e esp32dev-sim -t upload
```

✓ *Contrôle :* écran d'accueil, puis le chrono tourne tout seul ; toutes les
~40 s l'écran s'inverse avec un temps au tour. Le bouton **BOOT** change de
page (appui court) et remet la session à zéro (appui long sur la page SESSION).
C'est exactement le comportement qu'on aura sur la piste.

### Étape 2 — Le GPS

Toujours éteint, câble le GPS (soude d'abord sa rangée de pins si elle est
livrée en vrac) :

| GPS NEO-6M | ESP32 |
|---|---|
| VCC | VIN (5V) |
| GND | GND |
| TX | GPIO16 |
| RX | GPIO17 |

Flashe le firmware réel et ouvre la console :

```bash
pio run -e esp32dev -t upload
pio device monitor
```

Va **dehors** (ou près d'une fenêtre, antenne vers le ciel) et attends 1 à
5 min pour le premier fix (les suivants prennent quelques secondes).

✓ *Contrôle :* la console affiche `[GPS] configure : 57600 bauds, 200 ms/mesure` ;
sur la page GPS de l'écran : `FIX`, `5.0Hz`, 7+ satellites, HDOP < 2, et ta
position. La page RACE affiche ta vitesse.

### Étape 3 — Test grandeur nature (à vélo ou en scooter)

1. Choisis une boucle (parking, pâté de maisons).
2. Page LIGNE → roule à plus de 10 km/h → **appui long** en passant sur ta
   « ligne » imaginaire → `LIGNE DEFINIE !`.
3. Fais des tours (il faut des tours > 30 s et passer la ligne à > 15 km/h).

✓ *Contrôle :* chaque passage flashe le temps du tour ; la page SESSION liste
les tours ; `d` dans la console série sort le CSV.

### Étape 4 — L'alimentation batterie

⚠️ *L'unique étape où on peut griller quelque chose. Multimètre obligatoire.*

1. Soude la batterie sur le TP4056 : `B+`/`B-` (respecte la polarité !).
2. Soude `OUT+` du TP4056 → interrupteur → `IN+` du MT3608, et `OUT-` → `IN-`.
3. **Avant de brancher l'ESP32** : allume, mesure la sortie du MT3608 au
   multimètre et tourne la petite vis du potentiomètre jusqu'à lire
   **5,0-5,2 V** (compte plusieurs tours de vis, c'est normal).
4. Éteins, soude `OUT+` du MT3608 → **VIN** de l'ESP32 et `OUT-` → **GND**.

```
LiPo ──► TP4056 ──► interrupteur ──► MT3608 (réglé 5 V) ──► VIN + GND ESP32
          ▲
     micro-USB = recharge
```

✓ *Contrôle :* tout fonctionne sur batterie, sans câble USB. La recharge se
fait par le micro-USB du TP4056 (LED rouge = charge, bleue/verte = chargé).
On peut flasher par USB avec la batterie branchée, pas de conflit.

### Étape 5 — Boîtier et montage moto

1. Perce le boîtier : fenêtre écran (film plastique ou plexi collé derrière),
   passage micro-USB (recharge), trou du bouton étanche (GPIO25 → bouton → GND),
   interrupteur.
2. **Antenne GPS vers le ciel**, rien de métallique au-dessus, montée sur
   mousse 3M (antivibration). Le module GPS peut rester dans le boîtier si le
   couvercle est en plastique.
3. Colle chaude sur tous les connecteurs Dupont (ou soude-les : plus fiable
   sur une moto qui vibre), silicone sur les passages de câbles.
4. Adaptateur universel Quad Lock collé/vissé au dos → clipse sur le réservoir.

✓ *Contrôle final :* secoue le boîtier — rien ne bouge, rien ne sonne creux.

### Étape 6 — Le jour du track day

1. Allume l'appareil dans le paddock, ciel dégagé, 2-3 min avant d'entrer.
2. Premier roulage sur ce circuit : page LIGNE, **appui long en franchissant
   la vraie ligne** pendant le tour de reconnaissance. C'est mémorisé pour
   toutes les sessions et sorties suivantes.
3. Roule. Tout est automatique — ne touche plus à rien.
4. Le soir : branche l'USB, `pio device monitor`, tape `d`, copie tes temps
   (heures en UTC, Bangkok = UTC+7).

---

## 4. Utilisation — référence rapide

### Pages (appui court = page suivante)

| Page | Contenu | Appui long |
|---|---|---|
| **RACE** | tour en cours (gros), vitesse, sats, Dernier/Meilleur | — |
| **SESSION** | derniers tours (meilleur marqué `*`), vmax session | remise à zéro session |
| **GPS** | fix, satellites, HDOP, cadence Hz, position | — |
| **LIGNE** | état et distance de la ligne | définir la ligne ici |

Quand le chrono tourne, retour automatique sur RACE après 15 s. Les appuis
longs sont désactivés sur RACE et GPS pour éviter les fausses manips en
roulant.

### Commandes série (115200 bauds)

| Commande | Effet |
|---|---|
| `h` | aide |
| `i` | infos : ligne, session, record, état GPS |
| `d` | dump du journal CSV de tous les tours |
| `x` | efface le journal CSV |
| `r` | remise à zéro de la session |
| `z` | efface le record absolu |
| `L <lat> <lon> <cap> [demi-largeur]` | définit la ligne à la main (ex. depuis Google Maps) |

### Réglages principaux (`src/config.h`)

| Constante | Défaut | Rôle |
|---|---|---|
| `LINE_HALF_WIDTH_M` | 20 m | demi-largeur de la porte de détection |
| `MIN_LAP_MS` | 30 s | temps au tour minimum (anti-double détection) |
| `MIN_CROSS_SPEED_KMH` | 15 km/h | vitesse mini pour valider un passage |
| `GPS_MEAS_RATE_MS` | 200 ms | cadence GPS (5 Hz = max du NEO-6M) |
| `PIN_*` | — | brochage complet |

Écran monté tête en bas ? `U8G2_R0` → `U8G2_R2` dans `src/display.h`.

---

## 5. Le code, en bref

```
src/
├── main.cpp      Chef d'orchestre : boucle principale, boutons, commandes
│                 série, mode simulation
├── config.h      Tous les réglages et le brochage
├── gps.cpp/.h    Pilote le NEO-6M : détection auto du baud, configuration
│                 UBX (5 Hz, phrases inutiles coupées), production de "fixes"
│                 propres via TinyGPS++
├── laptimer.cpp/.h  L'algorithme : géométrie du franchissement de porte,
│                 interpolation du temps exact, tours/meilleur/deltas
├── display.cpp/.h   Les 4 pages OLED + flash de fin de tour (lib U8g2)
├── storage.cpp/.h   Persistance : ligne + record en NVS, journal CSV en
│                 LittleFS (flash interne)
└── button.cpp/.h    Anti-rebond, distinction appui court / appui long
```

Les points intéressants :

- **`gps.cpp`** parle au NEO-6M en binaire u-blox (UBX) au démarrage : il
  teste plusieurs vitesses jusqu'à l'entendre, le passe à 57600 bauds, coupe
  les phrases NMEA inutiles (ne garde que RMC + GGA) et monte la cadence à
  5 Hz. Si le module ne répond pas, tout marche quand même en mode dégradé 1 Hz.
- **`laptimer.cpp`** travaille en coordonnées locales (mètres autour de la
  ligne). Un tour = le segment entre deux fixes passe de « avant » à « après »
  la ligne, dans la porte, dans le bon sens. Le temps exact est interpolé
  linéairement entre les heures GPS des deux fixes — d'où la précision
  d'environ un dixième malgré le 5 Hz.
- **Deux environnements de build** : `esp32dev` (réel) et `esp32dev-sim`
  (circuit virtuel intégré — même code de chrono, positions générées — pour
  tester sans GPS).

---

## 6. Dépannage

| Symptôme | Cause probable |
|---|---|
| Écran noir | SDA/SCL inversés, ou module en adresse 0x3D (rare) : ajouter `u8g2_.setI2CAddress(0x3D * 2);` dans `Display::begin()` |
| Pas de port série | Câble USB « charge seule » (prends un câble data), ou pilote CH340 manquant (macOS récent : intégré) |
| `pas de NMEA detecte` dans la console | TX/RX inversés (croiser les fils), ou GPS mal alimenté |
| Pas de fix au bout de 10 min | Antenne sans vue du ciel (béton, métal au-dessus) ; premier démarrage à froid = jusqu'à 5 min dehors |
| Page GPS affiche ~1.0Hz | La config 5 Hz n'est pas passée : vérifier le câble RX (GPIO17 → RX du GPS), redémarrer dehors |
| Redémarrages aléatoires sur batterie | MT3608 mal réglé ou batterie vide — mesurer la tension sur VIN |
| Tours non détectés | Ligne définie au mauvais endroit/mauvais cap (redéfinir), ou tours < 30 s (`MIN_LAP_MS`) |

---

## 7. Évolutions possibles (v2)

- GPS u-blox **M8N/M10** (10-25 Hz, multi-constellations) : remplaçant direct,
  même câblage, même code → précision nettement meilleure (~200-400 ฿).
- Diffusion **BLE vers RaceChrono** (protocole « RaceChrono DIY ») : analyse
  pro des sessions sur téléphone, secteurs, traces.
- **Delta prédictif** en temps réel (comparaison continue avec le meilleur tour).
- Écran OLED **2,42"** SSD1309 : même bibliothèque, une ligne à changer.
- Jauge batterie (pont diviseur 2×100 kΩ vers une entrée ADC).

---

## Sécurité

Fixe l'appareil solidement (une pièce qui se détache à 150 km/h est un
projectile), ne le manipule jamais en roulant — tout est automatique une fois
la ligne définie — et vérifie le règlement du circuit concernant les appareils
embarqués.

*Projet DIY à ~600 ฿, aucune garantie — amuse-toi et progresse en sécurité.* 🏁
