# Scene import.
# Set the export directory below. It must contain Assets and Config, with the same layout as ExportData.
$ExportDirectory = "F:\Render\ShadowEngine\ExportData"

# Engine directory. Import clears Assets\Mesh here, then writes the cooked assets and config back.
$ProjectRoot = "F:\Render\ShadowEngine"

$Tool = Join-Path $ProjectRoot "build\bin\AssetImportTool.exe"
if (-not (Test-Path $Tool)) {
    Write-Error "Cannot find $Tool. Build AssetImportTool first."
    exit 1
}

& $Tool scene $ExportDirectory $ProjectRoot
exit $LASTEXITCODE
