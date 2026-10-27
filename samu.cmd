@echo off
rem Samu Monitor 串口交互 - 包装器（绕过 PowerShell 执行策略限制）
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0samu.ps1" %*
