# Clear the scene without importing.
# Engine directory. This empties Assets\Mesh and writes an empty Config\Scene.json.
$ProjectRoot = "F:\Render\ShadowEngine"

$Tool = Join-Path $ProjectRoot "build\bin\AssetImportTool.exe"
if (-not (Test-Path $Tool)) {
    Write-Error "Cannot find $Tool. Build AssetImportTool first."
    exit 1
}

& $Tool clear $ProjectRoot
exit $LASTEXITCODE
