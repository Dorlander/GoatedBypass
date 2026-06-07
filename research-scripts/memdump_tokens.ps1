# Dump playerTokens from Riot process memory
# Run as Administrator while Riot Client is running WITH Vanguard

Add-Type @"
using System;
using System.Runtime.InteropServices;

public class MemReader2 {
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

$outFile = "C:\Users\User\Desktop\memory_tokens.txt"
"" | Set-Content $outFile -Encoding utf8

$searchStrings = @('"playerTokens"', 'playerTokens', '"vanguard_session"')
$allFound = @()

foreach ($procName in @("LeagueClient", "RiotClientServices")) {
    $proc = Get-Process $procName -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $proc) {
        Write-Host "[-] $procName not running" -ForegroundColor DarkGray
        continue
    }

    Write-Host "[*] Scanning $($proc.ProcessName) PID=$($proc.Id)..." -ForegroundColor Cyan

    $handle = [MemReader2]::OpenProcess(0x0410, $false, $proc.Id)
    if ($handle -eq [IntPtr]::Zero) {
        Write-Host "[!] Cannot open $procName. Run as Administrator!" -ForegroundColor Red
        continue
    }

    $addr = [IntPtr]::Zero
    $mbi = New-Object MemReader2+MEMORY_BASIC_INFORMATION
    $mbiSize = [Runtime.InteropServices.Marshal]::SizeOf($mbi)
    $totalScanned = 0

    while ($true) {
        $result = [MemReader2]::VirtualQueryEx($handle, $addr, [ref]$mbi, $mbiSize)
        if ($result -eq 0) { break }

        if ($mbi.State -eq 0x1000 -and $mbi.RegionSize.ToInt64() -gt 0 -and $mbi.RegionSize.ToInt64() -lt 50MB) {
            $size = [int]$mbi.RegionSize.ToInt64()
            $buf = New-Object byte[] $size
            $bytesRead = 0

            if ([MemReader2]::ReadProcessMemory($handle, $mbi.BaseAddress, $buf, $size, [ref]$bytesRead) -and $bytesRead -gt 0) {
                $text = [System.Text.Encoding]::UTF8.GetString($buf, 0, $bytesRead)
                $totalScanned += $bytesRead

                foreach ($search in $searchStrings) {
                    $idx = $text.IndexOf($search)
                    while ($idx -ge 0) {
                        $start = [Math]::Max(0, $idx - 100)
                        $len = [Math]::Min(3000, $text.Length - $start)
                        $snippet = $text.Substring($start, $len)
                        $snippet = $snippet -replace '[^\x20-\x7E]', '.'

                        Write-Host "[+] FOUND in $procName at $($mbi.BaseAddress)+$idx" -ForegroundColor Green

                        $entry = "=== $procName PID=$($proc.Id) | '$search' at $($mbi.BaseAddress)+$idx ===`r`n$snippet`r`n"
                        $entry | Add-Content $outFile -Encoding utf8
                        $allFound += $entry

                        $idx = $text.IndexOf($search, $idx + 1)
                    }
                }
            }
        }

        $addr = [IntPtr]($mbi.BaseAddress.ToInt64() + $mbi.RegionSize.ToInt64())
        if ($addr.ToInt64() -lt 0) { break }
    }

    [MemReader2]::CloseHandle($handle) | Out-Null
    Write-Host "[*] Scanned $([Math]::Round($totalScanned / 1MB)) MB in $procName" -ForegroundColor Cyan
}

Write-Host "`n[*] Total matches: $($allFound.Count)" -ForegroundColor Cyan
Write-Host "[+] Saved to $outFile" -ForegroundColor Green
