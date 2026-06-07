# Continuously monitor memory for non-empty playerTokens
# Run as Administrator, then start a game

Add-Type @"
using System;
using System.Runtime.InteropServices;

public class MemMon {
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

$outFile = "C:\Users\User\Desktop\token_capture.txt"

function Scan-Process($procName) {
    $proc = Get-Process $procName -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $proc) { return $null }

    $handle = [MemMon]::OpenProcess(0x0410, $false, $proc.Id)
    if ($handle -eq [IntPtr]::Zero) { return $null }

    $addr = [IntPtr]::Zero
    $mbi = New-Object MemMon+MEMORY_BASIC_INFORMATION
    $mbiSize = [Runtime.InteropServices.Marshal]::SizeOf($mbi)
    $results = @()

    while ($true) {
        $r = [MemMon]::VirtualQueryEx($handle, $addr, [ref]$mbi, $mbiSize)
        if ($r -eq 0) { break }

        if ($mbi.State -eq 0x1000 -and $mbi.RegionSize.ToInt64() -gt 0 -and $mbi.RegionSize.ToInt64() -lt 50MB) {
            $size = [int]$mbi.RegionSize.ToInt64()
            $buf = New-Object byte[] $size
            $bytesRead = 0

            if ([MemMon]::ReadProcessMemory($handle, $mbi.BaseAddress, $buf, $size, [ref]$bytesRead) -and $bytesRead -gt 0) {
                $text = [System.Text.Encoding]::UTF8.GetString($buf, 0, $bytesRead)

                # Search for playerTokens that is NOT empty {}
                # Pattern: "playerTokens":{ followed by something other than }
                $idx = 0
                while ($true) {
                    $idx = $text.IndexOf('"playerTokens":', $idx)
                    if ($idx -lt 0) { break }

                    # Get 500 chars after the match
                    $start = $idx
                    $len = [Math]::Min(500, $text.Length - $start)
                    $snippet = $text.Substring($start, $len)
                    $snippet = $snippet -replace '[^\x20-\x7E]', '.'

                    # Check if NOT empty
                    $afterColon = $snippet.Substring('"playerTokens":'.Length).TrimStart()
                    if ($afterColon.StartsWith('{}') -or $afterColon.StartsWith('{},')) {
                        # empty - skip
                    } else {
                        $results += @{
                            process = $procName
                            pid = $proc.Id
                            address = "$($mbi.BaseAddress)+$idx"
                            snippet = $snippet
                        }
                    }

                    $idx++
                }
            }
        }

        $addr = [IntPtr]($mbi.BaseAddress.ToInt64() + $mbi.RegionSize.ToInt64())
        if ($addr.ToInt64() -lt 0) { break }
    }

    [MemMon]::CloseHandle($handle) | Out-Null
    return $results
}

Write-Host @"
=== Token Monitor ===
Scanning every 3 seconds for non-empty playerTokens.
Start a game NOW (Practice Tool recommended).

Press Ctrl+C to stop.

"@ -ForegroundColor Cyan

$round = 0
while ($true) {
    $round++
    $time = Get-Date -Format "HH:mm:ss"

    foreach ($procName in @("RiotClientServices", "LeagueClient", "vgc")) {
        $found = Scan-Process $procName
        if ($found -and $found.Count -gt 0) {
            Write-Host "[$time] !!! FOUND NON-EMPTY playerTokens in $procName !!!" -ForegroundColor Red -BackgroundColor Yellow

            foreach ($f in $found) {
                $entry = "=== [$time] $($f.process) PID=$($f.pid) at $($f.address) ===`r`n$($f.snippet)`r`n"
                Write-Host $entry -ForegroundColor Green
                $entry | Add-Content $outFile -Encoding utf8
            }

            Write-Host "[+] SAVED to $outFile" -ForegroundColor Green
            Write-Host "[+] TOKEN CAPTURED! You can stop now." -ForegroundColor Green -BackgroundColor DarkGreen
            # Keep scanning in case there are more
        }
    }

    Write-Host "[$time] Round $round - scanning..." -ForegroundColor DarkGray -NoNewline
    Write-Host "`r" -NoNewline
    Start-Sleep -Seconds 3
}
