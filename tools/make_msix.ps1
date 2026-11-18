<#
Windows side of the Store package (tools/make_msix.py builds it; this checks, signs a test copy,
installs it). Needs the Windows 10/11 SDK for MakeAppx.exe and SignTool.exe.

  # check the package with Microsoft's own reader (ZIP, block map, manifest schema)
  powershell -ExecutionPolicy Bypass -File tools\make_msix.ps1 -Msix dist\Cue-1.4-Store-x64.msix -Validate

  # or pack the layout from make_msix.py --layout with MakeAppx instead of the Python packer
  ... -Layout build\msix-layout -Msix dist\Cue-1.4-Store-x64.msix

  # a signed COPY for this PC (the Store file stays unsigned and untouched), then install it
  ... -Msix dist\Cue-1.4-Store-x64.msix -TestSign -Install

  # remove the test install before installing Cue from the Store
  ... -Uninstall

The test certificate's subject is the package's Publisher (CN=5166F7F3-...), as Windows requires.
Trusting it needs an administrator PowerShell once (the script prints the command if it is not one).
#>
param(
  [string]$Msix = "dist\Cue-1.4-Store-x64.msix",
  [string]$Layout,
  [switch]$Validate,
  [switch]$TestSign,
  [switch]$Install,
  [switch]$Uninstall
)
$ErrorActionPreference = "Stop"
$IdentityName = "tinypeep.Cueby.tinypeep"

function Find-SdkTool([string]$name) {
  $roots = @("${env:ProgramFiles(x86)}\Windows Kits\10\bin", "$env:ProgramFiles\Windows Kits\10\bin")
  $hit = foreach ($r in $roots) { if (Test-Path $r) { Get-ChildItem -Path $r -Recurse -Filter $name -ErrorAction SilentlyContinue | Where-Object { $_.FullName -match '\\x64\\' } } }
  $tool = $hit | Sort-Object { $v = [version]"0.0"; [void][version]::TryParse($_.Directory.Parent.Name, [ref]$v); $v } -Descending | Select-Object -First 1
  if (-not $tool) { $cmd = Get-Command $name -ErrorAction SilentlyContinue; if ($cmd) { return $cmd.Source } }
  if (-not $tool) { throw "$name not found: install the Windows SDK (MSIX Packaging Tools / Signing Tools)" }
  return $tool.FullName
}

function Read-Publisher([string]$package) {
  Add-Type -AssemblyName System.IO.Compression.FileSystem
  $zip = [System.IO.Compression.ZipFile]::OpenRead((Resolve-Path $package))
  try {
    $entry = $zip.GetEntry("AppxManifest.xml")
    $reader = New-Object System.IO.StreamReader($entry.Open())
    [xml]$m = $reader.ReadToEnd(); $reader.Close()
    return $m.Package.Identity.Publisher
  } finally { $zip.Dispose() }
}

if ($Uninstall) {
  # the sideloaded test copies only: Cue installed from the Store stays
  Get-AppxPackage -Name "$IdentityName*" | Where-Object { $_.SignatureKind -ne "Store" } | ForEach-Object { Write-Host "removing $($_.PackageFullName)"; Remove-AppxPackage $_.PackageFullName }
  return
}

if ($Layout) {
  $makeappx = Find-SdkTool "makeappx.exe"
  # resources.pri indexes the target-size, unplated icons; without it Windows puts the taskbar
  # and Start icon on a plate of the accent colour (needs "Windows SDK for UWP Managed Apps")
  $makepri = Find-SdkTool "makepri.exe"
  $cfg = Join-Path $env:TEMP "cue-priconfig.xml"
  & $makepri createconfig /cf $cfg /dq lang-en-US /pv 10.0.0 /o
  if ($LASTEXITCODE -ne 0) { throw "MakePri createconfig failed" }
  # one resources.pri for the whole package: no split per scale/language (that is for bundles)
  [xml]$x = Get-Content $cfg -Raw
  foreach ($n in @($x.resources.packaging)) { if ($n) { [void]$x.resources.RemoveChild($n) } }
  $x.Save($cfg)
  Get-ChildItem $Layout -Filter "resources*.pri" | Remove-Item -Force
  & $makepri new /pr $Layout /cf $cfg /mn (Join-Path $Layout "AppxManifest.xml") /of (Join-Path $Layout "resources.pri") /o
  if ($LASTEXITCODE -ne 0) { throw "MakePri new failed" }
  & $makeappx pack /o /h SHA256 /d $Layout /p $Msix
  if ($LASTEXITCODE -ne 0) { throw "MakeAppx pack failed" }
  $h = (Get-FileHash $Msix -Algorithm SHA256).Hash.ToLower()
  Set-Content -Path "$Msix.sha256" -Value "$h  $(Split-Path $Msix -Leaf)" -NoNewline -Encoding ascii
  Write-Host "$h  $Msix"
}

if ($Validate) {
  $makeappx = Find-SdkTool "makeappx.exe"
  $tmp = Join-Path $env:TEMP ("cue-msix-check-" + [guid]::NewGuid().ToString("N"))
  & $makeappx unpack /v /o /p $Msix /d $tmp
  $code = $LASTEXITCODE
  Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue
  if ($code -ne 0) { throw "MakeAppx could not read $Msix (see above). Pack the layout with -Layout instead." }
  Write-Host "ok: MakeAppx reads the package, its block map and its manifest"
}

if ($TestSign -or $Install) {
  $test = [IO.Path]::Combine((Split-Path (Resolve-Path $Msix)), [IO.Path]::GetFileNameWithoutExtension($Msix) + ".TESTE-assinado-local.msix")
  if ($TestSign) {
    $publisher = Read-Publisher $Msix
    $cert = Get-ChildItem Cert:\CurrentUser\My | Where-Object { $_.Subject -eq $publisher -and $_.FriendlyName -eq "Cue MSIX test" -and $_.NotAfter -gt (Get-Date) } | Select-Object -First 1
    if (-not $cert) {
      $cert = New-SelfSignedCertificate -Type Custom -Subject $publisher -KeyUsage DigitalSignature -FriendlyName "Cue MSIX test" `
        -CertStoreLocation "Cert:\CurrentUser\My" -NotAfter (Get-Date).AddYears(1) `
        -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3", "2.5.29.19={text}")
    }
    $cer = [IO.Path]::ChangeExtension($test, ".cer")
    Export-Certificate -Cert $cert -FilePath $cer | Out-Null
    Copy-Item $Msix $test -Force
    $signtool = Find-SdkTool "signtool.exe"
    & $signtool sign /fd SHA256 /sha1 $cert.Thumbprint /s My $test
    if ($LASTEXITCODE -ne 0) { throw "SignTool failed" }
    Write-Host "signed test copy: $test (certificate: $cer)"
    $trusted = Get-ChildItem Cert:\LocalMachine\TrustedPeople | Where-Object { $_.Thumbprint -eq $cert.Thumbprint }
    if (-not $trusted) {
      $admin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
      if ($admin) { Import-Certificate -FilePath $cer -CertStoreLocation Cert:\LocalMachine\TrustedPeople | Out-Null; Write-Host "test certificate trusted on this PC" }
      else { Write-Host "To trust the test certificate, run once in an administrator PowerShell:`n  Import-Certificate -FilePath `"$cer`" -CertStoreLocation Cert:\LocalMachine\TrustedPeople" }
    }
  }
  if ($Install) {
    if (-not (Test-Path $test)) { throw "no signed test copy: run with -TestSign first" }
    try { Add-AppxPackage -Path $test -ForceUpdateFromAnyVersion }
    catch {
      # a test copy of the same version with other contents (a rebuild): Windows will not
      # replace it in place, so the old test copy goes first (its own settings go with it)
      # (only a sideloaded test copy: Cue installed from the Store is never touched)
      $old = Get-AppxPackage -Name "$IdentityName*" | Where-Object { $_.SignatureKind -ne "Store" }
      if (-not $old) {
        if (Get-AppxPackage -Name "$IdentityName*") { Write-Host "Cue from the Store is installed: remove it first to try the test copy" }
        throw
      }
      Write-Host "replacing the earlier test copy of the same version ($($old.PackageFullName))"
      $old | Remove-AppxPackage
      Add-AppxPackage -Path $test -ForceUpdateFromAnyVersion
    }
    $p =Get-AppxPackage -Name "$IdentityName*" | Sort-Object InstallDate -Descending | Select-Object -First 1
    Write-Host "installed $($p.PackageFullName) in $($p.InstallLocation)"
    Write-Host "open it from Start (Cue), or: explorer.exe shell:AppsFolder\$($p.PackageFamilyName)!Cue"
  }
}
