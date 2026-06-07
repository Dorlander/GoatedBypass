# Dump all available LeagueClient API endpoints
# Run while the client is open WITH Vanguard

$lockfile = Get-Content "C:\Riot Games\League of Legends\lockfile" -ErrorAction SilentlyContinue
if (-not $lockfile) {
    # Try alternative paths
    $lockPaths = @(
        "C:\ProgramData\Riot Games\Metadata\league_of_legends.live\league_of_legends.live.lockfile"
    )
    foreach ($p in $lockPaths) {
        $lockfile = Get-Content $p -ErrorAction SilentlyContinue
        if ($lockfile) { break }
    }
}

if (-not $lockfile) {
    Write-Host "No lockfile found! Is LoL client running?"
    exit 1
}

$parts = $lockfile -split ':'
$port = $parts[2]
$pass = $parts[3]
$proto = $parts[4]

Write-Host "Port: $port, Protocol: $proto"

$pair = "riot:$pass"
$bytes = [System.Text.Encoding]::ASCII.GetBytes($pair)
$b64 = [System.Convert]::ToBase64String($bytes)

# Ignore SSL errors
Add-Type @"
using System.Net;
using System.Security.Cryptography.X509Certificates;
public class TrustAll : ICertificatePolicy {
    public bool CheckValidationResult(ServicePoint sp, X509Certificate cert, WebRequest req, int problem) { return true; }
}
"@
[System.Net.ServicePointManager]::CertificatePolicy = New-Object TrustAll
[System.Net.ServicePointManager]::SecurityProtocol = [System.Net.SecurityProtocolType]::Tls12

$headers = @{ "Authorization" = "Basic $b64" }
$base = "${proto}://127.0.0.1:${port}"

# Key endpoints to check for anticheat/vanguard/token data
$endpoints = @(
    "/lol-anti-cheat/v1/vanguard-session",
    "/anti-cheat/v1/session",
    "/anti-cheat/v1/vanguard-session",
    "/lol-gameflow/v1/session",
    "/lol-gameflow/v1/gameflow-metadata/registration-status",
    "/lol-gameflow/v1/gameflow-metadata/player-status",
    "/riotclient/anti-cheat/v1/session",
    "/riotclient/anti-cheat/v1/vanguard-session",
    "/entitlements/v1/token",
    "/lol-lobby/v2/registration/token",
    "/lol-lobby-team-builder/v1/session",
    "/player-session-lifecycle/v1/session",
    "/riotclient/v1/session",
    "/riotclient/region-locale",
    "/Help",
    "/swagger/v3/openapi/json",
    "/swagger/v2/swagger.json"
)

$outFile = "$PSScriptRoot\api_dump.json"
$results = @{}

foreach ($ep in $endpoints) {
    try {
        $resp = Invoke-RestMethod -Uri "${base}${ep}" -Headers $headers -Method Get -ErrorAction Stop
        Write-Host "[+] $ep" -ForegroundColor Green
        $results[$ep] = $resp
    } catch {
        $code = $_.Exception.Response.StatusCode.value__
        Write-Host "[-] $ep -> $code" -ForegroundColor DarkGray
    }
}

$results | ConvertTo-Json -Depth 10 | Out-File $outFile -Encoding utf8
Write-Host "`n[+] Saved to $outFile" -ForegroundColor Cyan

# Get full API schema and filter anticheat endpoints
$helpFile = "$PSScriptRoot\api_help.txt"
try {
    $help = Invoke-RestMethod -Uri "${base}/Help" -Headers $headers -Method Get -ErrorAction Stop
    $help | Out-File $helpFile -Encoding utf8
    Write-Host "`n[+] Full API help saved to $helpFile" -ForegroundColor Cyan

    $acEndpoints = $help | Where-Object { $_ -match "cheat|vanguard|token|anticheat|player-token" }
    Write-Host "`n=== Anticheat-related endpoints ===" -ForegroundColor Yellow
    foreach ($ep in $acEndpoints) {
        Write-Host $ep
    }
} catch {
    Write-Host "Help endpoint not available"
}
