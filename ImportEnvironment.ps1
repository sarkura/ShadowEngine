# Environment texture import.
# Set the absolute directory that contains one .hdr file.
$InputDirectory = "F:\Render\ShadowEngine\Assets\Environment"

# Engine directory. The cooked texture is written to Assets\Environment under this directory.
$ProjectRoot = "F:\Render\ShadowEngine"

$Tool = Join-Path $ProjectRoot "build\bin\AssetImportTool.exe"
if (-not (Test-Path $Tool)) {
    Write-Error "Cannot find $Tool. Build AssetImportTool first."
    exit 1
}

& $Tool environment $InputDirectory $ProjectRoot
exit $LASTEXITCODE
