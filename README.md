# 🎣 CYBER_FISHING v1.0
A cyberpunk-themed fishing RPG developed specifically for the Flipper Zero.

In the year 2026, data is the only currency that matters. Cast your line into the digital stream, reel in rare packets, and upgrade your hardware to breach the Internal Kernel.

# ✨ Features
5 Unique Sectors: Travel from the Cyber Docks to the Void Sector and the Internal Kernel.
Buzzer & Haptics: Full audio/visual feedback for bites and catches.
System Reformat (Prestige): Once your Net Index is full, reformat your core. Each version (v2, v3, etc.) grants a permanent 25% price bonus at the Market.
Hardware Upgrades: Improve your Buffer (longer reaction time), Antenna (better rarity), and Lure (faster bites).
Persistent Save System: Progress is saved automatically to your SD card (/apps_data/cyber_fishing.save).

# 🛠️ Installation & Build
Option 1: Build from Source (Recommended)
Ensure you have ufbt installed:
![test](./assets/cyber.PNG)

1. Open CMD/Terminal.
2. Enter pip install ufbt.

1. Clone the repo: git clone https://github.com/YOUR_USERNAME/cyber_fishing.git.
2. Connect your Flipper via USB.
3. Run: ufbt launch.

OR

1. Download repo
2. Ensure you have the cyber_fishing.c, application.fam, and icon_10.png files.
3. Use qFlipper or the Flipper mobile app to move the folder to SD Card/apps/Games/.

# 🎮 Controls
OK: Cast line / Reel in / Confirm
UP: Hardware Shop / Travel Menu
DOWN: Market (Sell Packets)
LEFT: Net Index (Collection & Prestige Status)
BACK: Return to menu / Exit (Saves automatically)
