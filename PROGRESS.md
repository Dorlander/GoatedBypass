# GoatedBypass — Progress Summary

## Phase 1: Network MITM Bypass — COMPLETE
- ConfigProxy intercepts `clientconfig.rpg.riotgames.com`
- JsonPatcher removes Vanguard requirement from config responses
- product_settings.yaml patcher disables local Vanguard dependency
- **Result:** Reach champion select without Vanguard, kicked at game start
- **Limitation:** Server-side check requires real Vanguard playerTokens

## Phase 2: Token Capture — INVESTIGATED, DEAD END
- TokenCapture queries LeagueClient REST API
- Memory scanning for playerTokens
- **Finding:** `playerTokens: {}` is ALWAYS empty in process memory
- **Conclusion:** Vanguard signs attestation at kernel level → server gets it from kernel directly, never through client memory

## Phase 3: Kernel Stealth Coexistence — COMPLETE
EFI bootkit + manually mapped driver that runs alongside Vanguard undetected.

### Components
- **xigmapper** (modified) — EFI bootkit loads our driver before vgk.sys
- **GoatedDriver v12.2** — stealth Ring 0 driver

### Achievements
- ✅ Loads BEFORE vgk via xigmapper
- ✅ Catches vgk via PsSetLoadImageNotifyRoutine
- ✅ Dumps full vgk.sys (51MB) to disk
- ✅ Removes own callback after capture
- ✅ Zero FF 25 import wrappers (passes vgk pattern scan)
- ✅ Vanguard does NOT detect us (no VAN 57)
- ✅ **Game launches and runs normally with Ring 0 driver loaded**

## Phase 4: vgk.sys Analysis (IDA Pro) — IN PROGRESS
- Full 51MB memory dump in IDA Pro 9.2
- Identified ~80 imports via `.stub0` section:
  - **BCrypt API** — RSA/SHA for attestation signing
  - **IoCreateFileEx** — file/pipe operations
  - **No network functions** — vgk doesn't talk to network directly
- DriverEntry is VM-obfuscated (Themida-style)
- Full reversing requires VM deobfuscation (months of work)

## What We Have Right Now
- **Working EFI bootkit** with any-driver loading capability
- **Stealth kernel driver** invisible to vgk
- **vgk.sys memory dump** for offline analysis
- **Ring 0 access** while playing League with Vanguard active

## Next Options

### Option 1: Use Ring 0 for Game Modifications
Read/modify League of Legends client memory from kernel.
Vanguard can't see our reads (we never touch its hooks).

### Option 2: Continue vgk Reverse Engineering
Use IDA Pro 9.2 + the dump to understand vgk protocol.
Build emulator. Months of work but feasible.

### Option 3: Automate Current Setup
One-click installer for bootkit + driver + Vanguard coexistence.

### Option 4: Document and Stop
Save as research achievement.

---

## Files Structure

```
Goated/
├── GoatedBypass/          # Phase 1-2: Userland MITM bypass
│   ├── ConfigProxy.cpp    # HTTPS proxy
│   ├── JsonPatcher.cpp    # Strip Vanguard from configs
│   ├── TokenCapture.cpp   # (dead end — tokens always empty)
│   └── RiotUtils.cpp      # Service kill, yaml patch
├── TestDriver/            # Phase 3: Stealth kernel driver
│   ├── driver.c           # v12.2 stealth driver source
│   └── README.md          # Architecture docs
├── PROGRESS.md            # This file
└── ...                    # Research scripts and dumps
```
