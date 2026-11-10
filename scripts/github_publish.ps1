param([switch]$DryRun,[switch]$CheckLocal,[switch]$Yes,[string]$ScopeFile='',
      [string]$Message='Unify tool build information and startup layout')
$ErrorActionPreference='Stop'
try {
    if (($DryRun -and $CheckLocal) -or ($Yes -and ($DryRun -or $CheckLocal))) { throw 'Select preview, local check or publication separately.' }
    . (Join-Path $PSScriptRoot 'github_publish_core.ps1')
    $root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..')).TrimEnd('\')
    $config=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'github_repository.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    if (!$config.publication_enabled -or !$config.repository -or !$config.visibility) {
        throw 'This project is local-only. Confirm/create the GitHub destination separately before enabling this updater. Nothing was staged, committed, created or pushed.'
    }
    if ($config.repository -notmatch '^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$' -or $config.visibility -notin @('PUBLIC','PRIVATE')) { throw 'Invalid pinned GitHub configuration.' }
    $legacyIdentity=Join-Path $PSScriptRoot 'repository_identity.json'
    if (Test-Path -LiteralPath $legacyIdentity) {
        $identity=Get-Content -LiteralPath $legacyIdentity -Raw -Encoding UTF8 | ConvertFrom-Json
        if (!$identity.publication_enabled -or $identity.github_repository -cne $config.repository) { throw 'Existing repository identity gate does not authorize this destination.' }
    }
    $target=@{Root=$root;Repository=$config.repository;Remote=('https://github.com/'+$config.repository+'.git');Branch=$config.branch;
        Title=$config.readme_title;IdentityFiles=@($config.identity_files);DeliveryFiles=@($config.delivery_files);Visibility=$config.visibility;HostOnly=$true}
    if ($ScopeFile) {
        $path=Assert-PublishPath $root $ScopeFile
        $scope=Get-Content -LiteralPath $path -Raw -Encoding UTF8 | ConvertFrom-Json
        if (!$scope.Count) { throw 'Exact publication scope cannot be empty.' }
        foreach ($relative in $scope) { if ($relative -isnot [string]) { throw 'Scope entries must be relative paths.' };[void](Assert-PublishPath $root $relative) }
        $target.ScopePaths=@($scope | ForEach-Object { $_.Replace('\','/') })
    }
    $plan=New-PublishPlan $target -LocalOnly:$CheckLocal
    Show-PublishPlan $plan
    if ($DryRun -or $CheckLocal) { exit 0 }
    if (!$Yes -and !(Test-PublishConfirmation (Read-Host 'Type PUSH to commit and push this reviewed scope'))) { Write-Output 'CANCELED: no index, commit or remote change.';exit 0 }
    Invoke-PublishPlan $plan $Message
    exit 0
} catch { Write-Error $_.Exception.Message -ErrorAction Continue;exit 1 }
