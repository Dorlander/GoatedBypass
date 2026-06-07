# Patch registration to mobileClient=true via local API
# Run while in lobby BEFORE starting the game

$lockfile = Get-Content "C:\Riot Games\League of Legends\lockfile" -ErrorAction SilentlyContinue
if (-not $lockfile) { $lockfile = Get-Content "C:\ProgramData\Riot Games\Metadata\league_of_legends.live\league_of_legends.live.lockfile" -ErrorAction SilentlyContinue }
if (-not $lockfile) { Write-Host "[!] No lockfile!"; exit 1 }

$parts = $lockfile -split ':'
$port = $parts[2]; $pass = $parts[3]; $proto = $parts[4]
$b64 = [System.Convert]::ToBase64String([System.Text.Encoding]::ASCII.GetBytes("riot:$pass"))

Add-Type @"
using System.Net;
using System.Security.Cryptography.X509Certificates;
public class TrustAll3 : ICertificatePolicy {
    public bool CheckValidationResult(ServicePoint sp, X509Certificate cert, WebRequest req, int problem) { return true; }
}
"@
[System.Net.ServicePointManager]::CertificatePolicy = New-Object TrustAll3
[System.Net.ServicePointManager]::SecurityProtocol = [System.Net.SecurityProtocolType]::Tls12

$headers = @{
    "Authorization" = "Basic $b64"
    "Content-Type" = "application/json"
}
$base = "${proto}://127.0.0.1:${port}"

# Step 1: Get current gameflow session to find party info
Write-Host "[*] Reading gameflow..." -ForegroundColor Cyan
try {
    $gf = Invoke-RestMethod "$base/lol-gameflow/v1/session" -Headers $headers -Method Get
    Write-Host "[+] Phase: $($gf.phase)" -ForegroundColor Green
} catch {
    Write-Host "[-] No gameflow session" -ForegroundColor Red
}

# Step 2: Try various endpoints to modify registration
$testEndpoints = @(
    @{ url = "/lol-lobby/v2/lobby/matchmaking/search"; method = "POST" },
    @{ url = "/riotclient/set_region_locale"; method = "PUT" }
)

# Step 3: Try patching via the internal parties API
Write-Host "`n[*] Attempting to patch mobileClient via local API..." -ForegroundColor Cyan

# Read all available endpoints with "registration" or "mobile"
$regEndpoints = @(
    "/lol-gameflow/v1/gameflow-metadata/registration-status",
    "/lol-lobby-team-builder/champ-select/v1/session",
    "/lol-lobby-team-builder/v1/matchmaking/search",
    "/lol-lobby/v2/registration/token"
)

foreach ($ep in $regEndpoints) {
    try {
        $resp = Invoke-RestMethod "$base$ep" -Headers $headers -Method Get -ErrorAction Stop
        Write-Host "[+] $ep" -ForegroundColor Green
        $resp | ConvertTo-Json -Depth 3 | Write-Host
    } catch {
        $code = $_.Exception.Response.StatusCode.value__
        Write-Host "[-] $ep -> $code" -ForegroundColor DarkGray
    }
}

# Step 4: Try writing mobileClient directly
Write-Host "`n[*] Trying direct PUT to parties registration..." -ForegroundColor Cyan

$regBody = @{
    playerTokens = @{}
    gameClientVersion = "16.11.7829736"
    mobileClient = $true
} | ConvertTo-Json

$putEndpoints = @(
    "/lol-lobby-team-builder/v1/lobby/registration",
    "/lol-lobby/v2/lobby/registration"
)

foreach ($ep in $putEndpoints) {
    try {
        $resp = Invoke-WebRequest "$base$ep" -Headers $headers -Method Put -Body $regBody -ErrorAction Stop
        Write-Host "[+] $ep -> $($resp.StatusCode)" -ForegroundColor Green
        Write-Host $resp.Content
    } catch {
        $code = $_.Exception.Response.StatusCode.value__
        Write-Host "[-] PUT $ep -> $code" -ForegroundColor DarkGray
    }
}

Write-Host "`n[*] Done. Now try starting the game!" -ForegroundColor Yellow
