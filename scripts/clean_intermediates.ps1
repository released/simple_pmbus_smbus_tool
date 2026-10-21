$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..')).TrimEnd('\')
$build = [IO.Path]::GetFullPath((Join-Path $projectRoot 'build'))
if ([IO.Path]::GetDirectoryName($build) -ne $projectRoot) { throw 'Invalid build root.' }

# Preflight every ancestor and descendant before deleting any compiler files.
$ancestor = $build
while ($ancestor) {
    if (Test-Path -LiteralPath $ancestor) {
        if (((Get-Item -LiteralPath $ancestor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Refusing to traverse a reparse point: $ancestor"
        }
    }
    $ancestor = [IO.Path]::GetDirectoryName($ancestor)
}
if (-not (Test-Path -LiteralPath $build)) { Write-Host 'No build intermediates.'; exit 0 }
$pending = [Collections.Generic.Stack[string]]::new()
$pending.Push($build)
$files = @()
$directories = @()
while ($pending.Count) {
    $item = Get-Item -LiteralPath $pending.Pop() -Force
    if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "Refusing to traverse a reparse point: $($item.FullName)"
    }
    if ($item.PSIsContainer) {
        $directories += $item
        foreach ($child in Get-ChildItem -LiteralPath $item.FullName -Force) {
            $pending.Push($child.FullName)
        }
    } else { $files += $item }
}

$extensions = @('.obj', '.pch', '.idb', '.pdb', '.ilk', '.res', '.iobj',
    '.ipdb', '.tlog', '.lastbuildstate')
$targets = @(Get-ChildItem -LiteralPath $projectRoot -Filter '*.vcxproj' -File |
    ForEach-Object { $_.BaseName })
$objectRoot = Join-Path $build 'obj'
$objectPrefix = $objectRoot + '\'
$removed = 0
foreach ($file in $files) {
    $inObjects = $file.FullName.StartsWith($objectPrefix, [StringComparison]::OrdinalIgnoreCase)
    $extension = $file.Extension.ToLowerInvariant()
    $known = $extension -in $extensions
    if ($extension -in @('.lib', '.exp')) {
        $known = $inObjects -or ($file.DirectoryName -eq $build -and $file.BaseName -in $targets)
    }
    if ($inObjects -and ($extension -in @('.recipe', '.rsp', '.manifest') -or
        $file.Name -like '*.vcxproj.FileListAbsolute.txt')) { $known = $true }
    if ($known) {
        Remove-Item -LiteralPath $file.FullName -Force
        $removed++
    }
}
# Never recursively purge a named folder. Even obj/tests may hold delivery data.
foreach ($directory in $directories | Sort-Object { $_.FullName.Length } -Descending) {
    if (($directory.FullName -eq $objectRoot -or
        $directory.FullName.StartsWith($objectPrefix, [StringComparison]::OrdinalIgnoreCase)) -and
        @(Get-ChildItem -LiteralPath $directory.FullName -Force).Count -eq 0) {
        Remove-Item -LiteralPath $directory.FullName -Force
    }
}
Write-Host "Clean complete: $removed allowlisted compiler files removed."
Write-Host 'Preserved EXE/INI/DLL/firmware/data/log/evidence/unknown files, including nested delivery folders.'
