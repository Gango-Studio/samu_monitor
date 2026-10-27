<#
    samu.ps1 - Samu Monitor 串口交互 (Pico USB CDC)

    用法:
        .\samu.cmd                 交互（推荐）
        .\samu.cmd -List           列出串口
        .\samu.cmd -Cmd "100.10F"  发一条命令
        .\samu.cmd -Port COM5      指定端口

    支持热插拔: 板子重新枚举后会自动重连（端口号变了也能找回来）。
    退出: Ctrl+C 或 Esc
#>
[CmdletBinding()]
param(
    [string]$Port,
    [int]$Baud = 115200,
    [int]$QuietMs = 800,
    [string]$Cmd,
    [switch]$List
)

$ErrorActionPreference = 'Stop'

function Get-SamuPorts {
    # 注册表里可能残留重复的端口项，必须去重
    try {
        return @([System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object -Unique)
    } catch {
        return @()
    }
}

function Connect-Samu {
    param([string]$prefer, [int]$baud, [switch]$Any)
    $ports = @(Get-SamuPorts)   # 必须 @()，否则单端口时会被拆成字符串
    if ($ports.Count -eq 0) { return $null }
    $target = $null
    if ($prefer -and ($ports -contains $prefer)) { $target = $prefer }
    elseif ($ports.Count -eq 1) { $target = $ports[0] }
    if (-not $target) { return $null }
    $p = New-Object System.IO.Ports.SerialPort $target, $baud, 'None', 8, 'One'
    $p.ReadTimeout = 500
    $p.WriteTimeout = 500
    $p.DtrEnable = $true
    $p.RtsEnable = $true
    try {
        $p.Open()
        return $p
    } catch {
        $p.Dispose()
        return $null
    }
}

function Read-UntilQuiet {
    param($sp, [int]$quiet)
    $sb = New-Object System.Text.StringBuilder
    $last = Get-Date
    while ((New-TimeSpan -Start $last -End (Get-Date)).TotalMilliseconds -lt $quiet) {
        try {
            if ($sp.BytesToRead -gt 0) {
                $n = $sp.BytesToRead
                $buf = New-Object byte[] $n
                $read = $sp.Read($buf, 0, $n)
                [void]$sb.Append([System.Text.Encoding]::ASCII.GetString($buf, 0, $read))
                $last = Get-Date
            } else {
                Start-Sleep -Milliseconds 20
            }
        } catch {
            break
        }
    }
    return $sb.ToString()
}

$all = @(Get-SamuPorts)

if ($List) {
    if ($all.Count -eq 0) { Write-Host "No COM port found." }
    else { Write-Host "Available COM ports:"; $all | ForEach-Object { Write-Host ("  " + $_) } }
    return
}

if (-not $Port -and $all.Count -gt 1) {
    Write-Host ("Multiple ports found: " + ($all -join ", ")) -ForegroundColor Yellow
    Write-Host "Pick one with: .\samu.cmd -Port COMx"
    return
}

$sp = Connect-Samu -prefer $Port -baud $Baud
if ($null -eq $sp) {
    if ($all.Count -eq 0) {
        Write-Host "No COM port found. Is the Pico plugged in?" -ForegroundColor Yellow
    } else {
        Write-Host ("Cannot open serial port. Available: " + ($all -join ", ")) -ForegroundColor Red
        Write-Host "Close other serial terminals (another samu.cmd / PuTTY / VS Code Serial Monitor) and retry."
    }
    return
}
$Port = $sp.PortName

try {
    # 连接瞬间设备已经发出的内容（例如开机横幅 =====Samu Monitor=====）
    $hello = Read-UntilQuiet -sp $sp -quiet 600
    if ($hello -and $hello.Trim().Length -gt 0) { Write-Host $hello -NoNewline }

    if ($Cmd) {
        try {
            $sp.DiscardInBuffer()
            $sp.Write($Cmd + [char]13 + [char]10)
            Start-Sleep -Milliseconds 60
        } catch {
            Write-Host ("Write failed: " + $_.Exception.Message) -ForegroundColor Red
        }
        Write-Output (Read-UntilQuiet -sp $sp -quiet $QuietMs)
        return
    }

    if ([Console]::IsInputRedirected) {
        while ($true) {
            $line = [Console]::ReadLine()
            if ($null -eq $line) { break }
            $line = $line.Trim()
            if ($line -eq 'exit' -or $line -eq 'quit') { break }
            if ($line -eq '') { continue }
            try { $sp.Write($line + [char]13 + [char]10) } catch { break }
            Start-Sleep -Milliseconds 60
            Write-Host (Read-UntilQuiet -sp $sp -quiet $QuietMs) -NoNewline
        }
    } else {
        $lost = $false
        while ($true) {
            # --- 断线自动重连 ---
            if ($null -eq $sp -or -not $sp.IsOpen) {
                if (-not $lost) {
                    Write-Host ""
                    Write-Host "[设备已断开，等待重连...]" -ForegroundColor DarkYellow
                    $lost = $true
                }
                $new = Connect-Samu -prefer $Port -baud $Baud -Any
                if ($null -eq $new) {
                    Start-Sleep -Milliseconds 200
                    continue
                }
                $sp = $new
                $Port = $sp.PortName
                $lost = $false
                Write-Host ("[已重新连接 " + $Port + "]") -ForegroundColor DarkGray
                continue
            }

            try {
                if ($sp.BytesToRead -gt 0) {
                    $n = $sp.BytesToRead
                    $buf = New-Object byte[] $n
                    $read = $sp.Read($buf, 0, $n)
                    Write-Host ([System.Text.Encoding]::ASCII.GetString($buf, 0, $read)) -NoNewline
                }

                $send = $null
                if ([Console]::KeyAvailable) {
                    $k = [Console]::ReadKey($true)
                    if ($k.Key -eq 'Enter') { $send = [string][char]13 + [char]10 }
                    elseif ($k.Key -eq 'Backspace') { $send = [string][char]8 }
                    elseif ($k.Key -eq 'Escape') { break }
                    elseif ([int]$k.KeyChar -ge 32 -and [int]$k.KeyChar -lt 127) { $send = [string]$k.KeyChar }
                }
                if ($null -ne $send) { $sp.Write($send) }
            } catch {
                try { $sp.Close(); $sp.Dispose() } catch { }
                $sp = $null
            }
            Start-Sleep -Milliseconds 15
        }
    }
}
finally {
    if ($null -ne $sp) {
        try { if ($sp.IsOpen) { $sp.Close() }; $sp.Dispose() } catch { }
    }
}
