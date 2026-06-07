# GoatedDriver — Stealth Kernel Driver

Ring 0 kernel driver that coexists with Riot Vanguard (vgk.sys) without triggering detection.

## Architecture

```
BIOS Boot
  ↓
xigmapper (EFI bootkit) — loads BEFORE Windows kernel
  ↓
Maps GoatedDriver.sys into kernel memory at IopInitializeSystemDrivers
  ↓
DriverEntry runs BEFORE vgk.sys loads
  ↓
Registers PsSetLoadImageNotifyRoutine callback
  ↓
StealthThread spawned
  ↓
vgk.sys loads → callback fires → captures base address + size
  ↓
StealthThread dumps vgk.sys → C:\vgk_dump.bin
  ↓
PsRemoveLoadImageNotifyRoutine — unregisters callback
  ↓
Driver enters silent sleep loop — invisible to vgk
```

## Anti-Detection Techniques

### 1. Manual Mapping (via xigmapper)
- Not registered in `PsLoadedModuleList`
- Not in service registry
- Driver object never created
- Bypasses `MmGetSystemRoutineAddress` enumeration

### 2. Zero FF 25 Import Wrappers
- vgk.sys scans memory for `FF 25` opcodes (MSVC-generated `jmp [rel]`)
- Only one `__declspec(dllimport)` used (MmGetSystemRoutineAddress)
- MSVC optimization eliminates wrapper in release build
- Verified: `dumpbin /disasm` shows zero FF 25 sequences

### 3. Stealth Phase After Capture
- Removes own callback from `PsImageNotifyRoutineList`
- No NPFS hooks
- No driver object modifications
- Only one-time file I/O (dump vgk + small log)
- After capture: only `KeDelayExecutionThread` calls

### 4. Defensive Programming
- All kernel pointers validated (>= 0xFFFF800000000000)
- `MmIsAddressValid` used before every memory dereference
- IRP structure walks bounded and length-checked
- No recursion in object chain walks

## Why Vanguard Doesn't Detect Us

| vgk Detection Method | Our Defense |
|---|---|
| PsLoadedModuleList enumeration | Not registered (manual mapping) |
| FF 25 pattern scan | Zero wrappers |
| PsImageNotifyRoutineList check | Callback removed after use |
| Driver object enumeration | No driver object created |
| Hooks on filesystem drivers | We don't install any |
| Boot integrity check | xigmapper preserves bootmgr |

## What This Achieves

**This is NOT a Vanguard bypass.** Vanguard still loads and attests to Riot's servers normally. The game launches and runs correctly.

**This IS a stealth Ring 0 driver** invisible to vgk.sys's scanning techniques. It's the foundation for:
- Game memory reading without anti-cheat detection
- Custom kernel-level modifications
- Further vgk.sys reverse engineering

## Build

```powershell
cl /c /GS- /Oi- /Od /W0 /Gs1000000 /D_AMD64_ /Zl /Fo:driver.obj driver.c
link driver.obj /OUT:GoatedDriver.sys /SUBSYSTEM:NATIVE /DRIVER:WDM /ENTRY:DriverEntry ^
  /LIBPATH:wdklib /LIBPATH:vclib ntoskrnl.lib /NODEFAULTLIB /DYNAMICBASE
```

Verify no import wrappers:
```powershell
$bytes = [System.IO.File]::ReadAllBytes("GoatedDriver.sys")
$count = 0
for ($i = 0; $i -lt $bytes.Length - 1; $i++) {
    if ($bytes[$i] -eq 0xFF -and $bytes[$i+1] -eq 0x25) { $count++ }
}
# Must be 0
```

## Deploy

1. Build with above commands
2. Place `GoatedDriver.sys` on `C:\` (not USB — USB loads after vgk)
3. Edit `xigmapper/hook.c` `g_module_path` to point to `C:\GoatedDriver.sys`
4. Build xigmapper, place `efi_driver.efi` on EFI partition (Z:)
5. Reboot to BIOS → boot from EFI Shell
6. Run `efi_driver.efi` then boot Windows: `EFI\Microsoft\Boot\bootmgfw.efi`

## Verification

After boot, check:
- `C:\goated_log.txt` should contain: `[v12] caught + dumped + going stealth`
- `C:\vgk_dump.bin` should be ~50MB
- Riot Client should launch without VAN 57

## Output

- `C:\vgk_dump.bin` — Full vgk.sys memory dump for IDA Pro analysis
- `C:\goated_log.txt` — One-line status log

## Source File

`driver.c` — Complete v12.2 stealth driver (~6KB compiled, no CRT, no imports beyond MmGetSystemRoutineAddress)
