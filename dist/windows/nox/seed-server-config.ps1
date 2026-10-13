<#
  Ap dung cau hinh toi uu UPLOAD cho qbittorrent-nox (server seed game update).
  Chay 1 lan sau khi qbittorrent-nox da chay:
    powershell -ExecutionPolicy Bypass -File seed-server-config.ps1 -Url http://127.0.0.1:8080 -User admin -Pass "matkhau"
  Neu WebUI bat "Bypass authentication for clients on localhost" thi co the bo -User/-Pass.
#>
param(
    [string]$Url = "http://127.0.0.1:8080",
    [string]$User = "",
    [string]$Pass = ""
)

$ErrorActionPreference = "Stop"
$api = $Url.TrimEnd('/') + "/api/v2"
$session = New-Object Microsoft.PowerShell.Commands.WebRequestSession

if ($User -ne "") {
    $r = Invoke-WebRequest -Uri "$api/auth/login" -Method Post -WebSession $session -UseBasicParsing `
        -Body @{ username = $User; password = $Pass }
    if ($r.Content -ne "Ok.") { throw "Dang nhap that bai: $($r.Content)" }
}

$prefs = [ordered]@{
    # --- Khong gioi han so torrent seed cung luc (300+ torrent) ---
    queueing_enabled                      = $false
    # --- Seed mai mai, khong tu dung theo ratio / thoi gian ---
    max_ratio_enabled                     = $false
    max_seeding_time_enabled              = $false
    max_inactive_seeding_time_enabled     = $false
    # --- Ket noi & slot upload ---
    max_connec                            = 5000
    max_connec_per_torrent                = 500
    max_uploads                           = -1      # khong gioi han slot upload
    max_uploads_per_torrent               = -1
    upload_slots_behavior                 = 0       # Fixed slots
    upload_choking_algorithm              = 1       # Fastest upload
    enable_multi_connections_from_same_ip = $true   # nhieu may tram sau cung 1 IP (NAT chi nhanh)
    up_limit                              = 0       # khong gioi han toc do upload
    # --- Hieu nang doc dia / gui du lieu ---
    file_pool_size                        = 5000    # nhieu file game mo cung luc
    async_io_threads                      = 32
    send_buffer_watermark                 = 5120    # KiB
    send_buffer_low_watermark             = 1024    # KiB
    send_buffer_watermark_factor          = 150     # %
    socket_backlog_size                   = 3000
    connection_speed                      = 50
    enable_upload_suggestions             = $true
    # --- Mang ---
    lsd                                   = $true   # tim peer trong LAN
    resolve_peer_countries                = $true
    geo_allowed_countries                 = "VN"    # chi cho phep peer Viet Nam (+ mang LAN noi bo)
    # --- WebAPI ---
    refresh_interval                      = 1000    # ms, du lieu API moi hon
}

$json = $prefs | ConvertTo-Json -Compress
Invoke-WebRequest -Uri "$api/app/setPreferences" -Method Post -WebSession $session -UseBasicParsing `
    -Body @{ json = $json } | Out-Null

$applied = (Invoke-WebRequest -Uri "$api/app/preferences" -WebSession $session -UseBasicParsing).Content | ConvertFrom-Json
Write-Host "Da ap dung. Kiem tra:" -ForegroundColor Green
foreach ($k in $prefs.Keys) {
    $v = $applied.$k
    $ok = ("$v" -eq "$($prefs[$k])")
    $mark = if ($ok) { "OK " } else { "!! " }
    Write-Host ("{0}{1,-40} = {2}" -f $mark, $k, $v)
}
Write-Host ("seed_only_mode = {0}" -f $applied.seed_only_mode)
