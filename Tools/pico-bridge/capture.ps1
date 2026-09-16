# Log the capture-mode serial port to a file on Windows.
#   powershell -ExecutionPolicy Bypass -File capture.ps1 -Port COM5
# Any key you press is written into the log as "# mark <key>", so you can tag
# what you are about to do on the controller. Ctrl+C stops.
param([string]$Port = 'COM3', [string]$Out = 'capture.log')

$sp = [System.IO.Ports.SerialPort]::new($Port, 115200)
$sp.DtrEnable = $true   # the board only starts printing once DTR is set
$sp.ReadTimeout = 50
$sp.Open()
$log = [System.IO.StreamWriter]::new((Join-Path (Get-Location) $Out))
$log.AutoFlush = $true
Write-Host "Logging $Port to $Out. Press a key to mark the log, Ctrl+C to stop."

$count = 0
$shown = [Diagnostics.Stopwatch]::StartNew()
try {
    while ($true) {
        if ([Console]::KeyAvailable) {
            $m = "# mark " + [Console]::ReadKey($true).KeyChar
            $log.WriteLine($m)
            Write-Host $m -ForegroundColor Yellow
        }
        try { $line = $sp.ReadLine().TrimEnd("`r") } catch [TimeoutException] { continue }
        $log.WriteLine($line)
        $count++
        # Echoing every report slows the loop enough to make the board drop
        # reports, so show only the latest one a few times a second.
        if ($line.StartsWith('#') -or $line.StartsWith('OGX') -or $shown.ElapsedMilliseconds -gt 250) {
            Write-Host "[$count] $line"
            $shown.Restart()
        }
    }
} finally {
    $sp.Close()
    $log.Close()
}
