param(
    [ValidateRange(1, 60)][int]$Seconds = 30,
    [ValidatePattern('^[a-zA-Z0-9_.]+$')][string]$Package = 'com.paiheulevrai.choochootracker',
    [ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Label = 'audio',
    [string]$Adb = "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe"
)

$ErrorActionPreference = 'Stop'
function Invoke-Adb {
    & $Adb @args
    if ($LASTEXITCODE -ne 0) { throw "adb failed: $args" }
}

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$captureDir = Join-Path $PSScriptRoot "..\.tmp\android-audio-$stamp-$Label"
New-Item -ItemType Directory -Path $captureDir | Out-Null
$remoteTrace = "/data/misc/perfetto-traces/cct-$stamp-$Label.perfetto-trace"

Invoke-Adb shell pidof $Package | Set-Content "$captureDir\pid.txt"
Invoke-Adb shell date | Set-Content "$captureDir\time.txt"
Invoke-Adb shell dumpsys media.audio_flinger | Set-Content "$captureDir\audioflinger-before.txt"
Write-Host "Recording $Seconds seconds of $Label. Keep the song playing and note audible glitches."
Invoke-Adb shell perfetto -t "${Seconds}s" -b 64mb -o $remoteTrace sched freq idle audio -a $Package
Invoke-Adb shell dumpsys media.audio_flinger | Set-Content "$captureDir\audioflinger-after.txt"
Invoke-Adb logcat -d -t 12000 -v threadtime 'CCTAudio:V' 'AudioTrack:V' 'AudioFlinger:V' 'SDL:V' 'SDL/APP:V' 'SDLAudio:V' 'AAudio:V' '*:S' |
    Set-Content "$captureDir\audio-logcat.txt"
Invoke-Adb pull $remoteTrace "$captureDir\trace.perfetto-trace"
# Remove only this script's trace, after a successful pull.
Invoke-Adb shell rm $remoteTrace
Write-Host "Capture: $((Resolve-Path $captureDir).Path)"
