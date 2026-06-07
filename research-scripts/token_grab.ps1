# Grab raw playerTokens from memory - bigger window, raw bytes
# Run as Admin while IN GAME

Add-Type @"
using System;
using System.Runtime.InteropServices;

public class MemGrab {
    [DllImport("kernel32.dll")]
    public static extern IntPtr OpenProcess(int access, bool inherit, int pid);

    [DllImport("kernel32.dll")]
    public static extern bool ReadProcessMemory(IntPtr hProcess, IntPtr baseAddr, byte[] buffer, int size, out int read);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern int VirtualQueryEx(IntPtr hProcess, IntPtr addr, out MEMORY_BASIC_INFORMATION mbi, int len);

    [DllImport("kernel32.dll")]
    public static extern bool CloseHandle(IntPtr h);

    [StructLayout(LayoutKind.Sequential)]
    public struct MEMORY_BASIC_INFORMATION {
        public IntPtr BaseAddress;
        public IntPtr AllocationBase;
        public uint AllocationProtect;
        public IntPtr RegionSize;
        public uint State;
        public uint Protect;
        public uint Type;
    }
}
"@

$proc = Get-Process "RiotClientServices" -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $proc) { Write-Host "[!] RiotClientServices not running!"; exit 1 }

Write-Host "[*] Scanning PID=$($proc.Id)..." -ForegroundColor Cyan

$handle = [MemGrab]::OpenProcess(0x0410, $false, $proc.Id)
if ($handle -eq [IntPtr]::Zero) { Write-Host "[!] Need admin!"; exit 1 }

$addr = [IntPtr]::Zero
$mbi = New-Object MemGrab+MEMORY_BASIC_INFORMATION
$mbiSize = [Runtime.InteropServices.Marshal]::SizeOf($mbi)
$count = 0

while ($true) {
    $r = [MemGrab]::VirtualQueryEx($handle, $addr, [ref]$mbi, $mbiSize)
    if ($r -eq 0) { break }

    if ($mbi.State -eq 0x1000 -and $mbi.RegionSize.ToInt64() -gt 0 -and $mbi.RegionSize.ToInt64() -lt 50MB) {
        $size = [int]$mbi.RegionSize.ToInt64()
        $buf = New-Object byte[] $size
        $bytesRead = 0

        if ([MemGrab]::ReadProcessMemory($handle, $mbi.BaseAddress, $buf, $size, [ref]$bytesRead) -and $bytesRead -gt 0) {
            $text = [System.Text.Encoding]::UTF8.GetString($buf, 0, $bytesRead)

            $idx = 0
            while ($true) {
                $idx = $text.IndexOf('"playerTokens":', $idx)
                if ($idx -lt 0) { break }

                $afterColon = $text.Substring($idx + 16, [Math]::Min(10, $text.Length - $idx - 16)).TrimStart()
                if (-not $afterColon.StartsWith('{}')) {
                    $count++
                    # Dump 10KB raw from this offset
                    $rawStart = $idx
                    $rawLen = [Math]::Min(10000, $bytesRead - $rawStart)
                    $rawBytes = New-Object byte[] $rawLen
                    [Array]::Copy($buf, $rawStart, $rawBytes, 0, $rawLen)

                    $outRaw = "C:\Users\User\Desktop\token_raw_$count.bin"
                    [System.IO.File]::WriteAllBytes($outRaw, $rawBytes)

                    # Also save cleaned text
                    $cleanText = [System.Text.Encoding]::UTF8.GetString($rawBytes)
                    $cleanText = $cleanText -replace '[^\x20-\x7E\r\n]', ''
                    $outTxt = "C:\Users\User\Desktop\token_clean_$count.txt"
                    [System.IO.File]::WriteAllText($outTxt, $cleanText)

                    Write-Host "[+] Match $count at $($mbi.BaseAddress)+$idx" -ForegroundColor Green
                    Write-Host "    Raw: $outRaw ($rawLen bytes)" -ForegroundColor Yellow
                    Write-Host "    Clean: $outTxt" -ForegroundColor Yellow
                }
                $idx++
            }
        }
    }

    $addr = [IntPtr]($mbi.BaseAddress.ToInt64() + $mbi.RegionSize.ToInt64())
    if ($addr.ToInt64() -lt 0) { break }
}

[MemGrab]::CloseHandle($handle) | Out-Null
Write-Host "`n[*] Found $count non-empty matches" -ForegroundColor Cyan
