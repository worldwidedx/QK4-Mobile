@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0build-android.ps1" %*
