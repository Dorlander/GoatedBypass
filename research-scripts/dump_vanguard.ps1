# Query vanguard-specific endpoints
$lockfile = Get-Content "C:\Riot Games\League of Legends\lockfile" -ErrorAction SilentlyContinue
if (-not $lockfile) { $lockfile = Get-Content "C:\ProgramData\Riot Games\Metadata\league_of_legends.live\league_of_legends.live.lockfile" -ErrorAction SilentlyContinue }
if (-not $lockfile) { Write-Host "No lockfile!"; exit 1 }

$parts = $lockfile -split ':'
$port = $parts[2]; $pass = $parts[3]; $proto = $parts[4]

$b64 = [System.Convert]::ToBase64String([System.Text.Encoding]::ASCII.GetBytes("riot:$pass"))

Add-Type @"
using System.Net;
using System.Security.Cryptography.X509Certificates;
public class TrustAll2 : ICertificatePolicy {
    public bool CheckValidationResult(ServicePoint sp, X509Certificate cert, WebRequest req, int problem) { return true; }
}
"@
[System.Net.ServicePointManager]::CertificatePolicy = New-Object TrustAll2
[System.Net.ServicePointManager]::SecurityProtocol = [System.Net.SecurityProtocolType]::Tls12

$headers = @{ "Authorization" = "Basic $b64" }
$base = "${proto}://127.0.0.1:${port}"

$endpoints = @(
    "/lol-vanguard/v1/session",
    "/lol-vanguard/v1/notification",
    "/lol-vanguard/v1/service-session-check-failure",
    "/lol-gameflow/v1/gameflow-metadata/registration-status",
    "/lol-gameflow/v1/gameflow-metadata/player-status",
    "/lol-lobby-team-builder/v1/session",
    "/lol-anti-addiction/v1/anti-addiction-token"
)

$results = @{}
foreach ($ep in $endpoints) {
    try {
        $resp = Invoke-RestMethod -Uri "${base}${ep}" -Headers $headers -Method Get -ErrorAction Stop
        Write-Host "[+] $ep" -ForegroundColor Green
        $results[$ep] = $resp
    } catch {
        $code = $_.Exception.Response.StatusCode.value__
        Write-Host "[-] $ep -> $code" -ForegroundColor Red
    }
}

$outFile = "$PSScriptRoot\vanguard_dump.json"
$results | ConvertTo-Json -Depth 10 | Out-File $outFile -Encoding utf8
Write-Host "`n[+] Saved to $outFile" -ForegroundColor Cyan
