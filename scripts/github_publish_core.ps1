# Original local publisher adaptation: pinned visibility, host-only updates and optional exact scope.
function Get-PublishExpectedVisibility($Target) {
    if ($Target.ContainsKey('Visibility')) { return $Target.Visibility.ToUpperInvariant() }
    return 'PRIVATE'
}
function Get-PublishObservedVisibility($Remote) {
    if ($Remote.ContainsKey('Visibility')) { return $Remote.Visibility.ToUpperInvariant() }
    return $(if ($Remote.Private) { 'PRIVATE' } else { 'PUBLIC' })
}
function Test-PublishHostWithheld($Target,[string]$Path) {
    return ($Target.ContainsKey('HostOnly') -and $Target.HostOnly -and $Path -match '^(firmware|demo_code|mcu|sample_projects)/')
}
# Project-local Git publication. No build, device access or repository creation.
function Test-PublishConfirmation([string]$Answer) {
    return $Answer -ceq 'PUSH'
}

function Invoke-PublishProcess([string]$Program, [string[]]$Arguments, [int[]]$Allowed = @(0)) {
    $info = New-Object Diagnostics.ProcessStartInfo
    $info.FileName = (Get-Command $Program -CommandType Application -ErrorAction Stop).Source
    $info.Arguments = ($Arguments | ForEach-Object {
        '"' + [regex]::Replace([regex]::Replace($_,'(\\*)"','$1$1\"'),'(\\+)$','$1$1') + '"'
    }) -join ' '
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $info.StandardOutputEncoding = New-Object Text.UTF8Encoding($false)
    $info.StandardErrorEncoding = New-Object Text.UTF8Encoding($false)
    $info.EnvironmentVariables['GIT_TERMINAL_PROMPT'] = '0'
    $info.EnvironmentVariables['GCM_INTERACTIVE'] = 'Never'
    $info.EnvironmentVariables['GIT_OPTIONAL_LOCKS'] = '0'
    $process = New-Object Diagnostics.Process
    $process.StartInfo = $info
    try {
        [void]$process.Start()
        $output = $process.StandardOutput.ReadToEndAsync()
        $errors = $process.StandardError.ReadToEndAsync()
        $process.WaitForExit()
        $result = @{ Code=$process.ExitCode; Text=$output.Result.TrimEnd(); Error=$errors.Result.TrimEnd() }
        if ($result.Code -notin $Allowed) { throw "$Program $($Arguments[0]) failed: $($result.Error)" }
        return $result
    } finally { $process.Dispose() }
}

function Invoke-PublishGit([string]$Root, [string[]]$Arguments, [int[]]$Allowed = @(0)) {
    foreach ($name in @('GIT_DIR','GIT_WORK_TREE','GIT_COMMON_DIR','GIT_INDEX_FILE','GIT_OBJECT_DIRECTORY',
        'GIT_ALTERNATE_OBJECT_DIRECTORIES','GIT_NAMESPACE','GIT_CONFIG_PARAMETERS')) {
        if ([Environment]::GetEnvironmentVariable($name)) { throw "Git environment override refused: $name" }
    }
    if ($env:GIT_CONFIG_COUNT) {
        if ($env:GIT_CONFIG_COUNT -notmatch '^([0-9]|[1-5][0-9]|6[0-4])$') { throw 'Invalid GIT_CONFIG_COUNT.' }
        # Managed terminals may supply ownership trust, never transport/hooks overrides.
        for ($i=0; $i -lt [int]$env:GIT_CONFIG_COUNT; $i++) {
            if ([Environment]::GetEnvironmentVariable("GIT_CONFIG_KEY_$i") -cne 'safe.directory') {
                throw 'Only inherited safe.directory Git configuration is allowed.'
            }
        }
    }
    return Invoke-PublishProcess git (@('--literal-pathspecs','-C',$Root,'-c','core.quotepath=false') + $Arguments) $Allowed
}

function Assert-PublishPath([string]$Root, [string]$Relative) {
    if ([IO.Path]::IsPathRooted($Relative) -or $Relative.Contains(':') -or $Relative -match '(^|[/\\])\.\.([/\\]|$)') {
        throw "Non-relative publication path: $Relative"
    }
    $rootPath = [IO.Path]::GetFullPath($Root).TrimEnd('\')
    $path = [IO.Path]::GetFullPath((Join-Path $rootPath $Relative))
    if ($path -ine $rootPath -and !$path.StartsWith($rootPath+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Path escaped project.' }
    $part = $path
    while ($part) {
        if ((Test-Path -LiteralPath $part) -and ((Get-Item -LiteralPath $part -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "Reparse path refused: $Relative"
        }
        $part = [IO.Path]::GetDirectoryName($part)
    }
    return $path
}

function Get-PublishRemoteKey([string]$Remote) {
    if ($Remote -match '^https://github\.com/([A-Za-z0-9_.-]+)/([A-Za-z0-9_.-]+?)(?:\.git)?/?$' -or
        $Remote -match '^git@github\.com:([A-Za-z0-9_.-]+)/([A-Za-z0-9_.-]+?)(?:\.git)?/?$' -or
        $Remote -match '^ssh://git@github\.com/([A-Za-z0-9_.-]+)/([A-Za-z0-9_.-]+?)(?:\.git)?/?$') {
        return ($Matches[1]+'/'+$Matches[2]).ToLowerInvariant()
    }
    # Local bare paths are used only by core-level, no-network fixtures.
    if ([IO.Path]::IsPathRooted($Remote)) { return [IO.Path]::GetFullPath($Remote).TrimEnd('\').ToLowerInvariant() }
    throw 'Unsupported remote; credentials, indirect URLs and unpinned destinations are refused.'
}

function Get-PublishIndex([string]$Root) {
    $entries = @{}
    foreach ($record in (Invoke-PublishGit $Root @('ls-files','--stage','-z')).Text.Split([char]0)) {
        if (!$record) { continue }
        $parts = $record.Split(@([char]9),2)
        $meta = $parts[0].Split(' ')
        if ($meta[2] -ne '0' -or $meta[0] -notin @('100644','100755')) { throw 'Unmerged/symlink/submodule index requires review.' }
        $entries[$parts[1]] = @{ Mode=$meta[0]; Sha=$meta[1] }
    }
    foreach ($record in (Invoke-PublishGit $Root @('ls-files','-v','-z')).Text.Split([char]0)) {
        if ($record -and $record[0] -cne 'H') { throw 'Hidden index flags require review.' }
    }
    return $entries
}

function Assert-PublishRepository($Target) {
    $root = $Target.Root
    [void](Assert-PublishPath $root '.')
    if (!(Test-Path -LiteralPath (Assert-PublishPath $root '.git') -PathType Container)) { throw 'A dedicated .git directory is required.' }
    if ((Invoke-PublishGit $root @('rev-parse','--show-toplevel')).Text.Replace('/','\').TrimEnd('\') -ine $root) { throw 'Wrong repository root.' }
    if ((Invoke-PublishGit $root @('branch','--show-current')).Text -cne $Target.Branch) { throw "Expected branch: $($Target.Branch)" }
    $key = Get-PublishRemoteKey $Target.Remote
    foreach ($arguments in @(@('remote','get-url','--all','origin'),@('remote','get-url','--push','--all','origin'))) {
        $urls = @((Invoke-PublishGit $root $arguments).Text -split '\r?\n' | Where-Object { $_ })
        if ($urls.Count -ne 1 -or (Get-PublishRemoteKey $urls[0]) -cne $key) { throw 'Origin fetch/push destination mismatch.' }
    }
    if ((Invoke-PublishGit $root @('config','--bool','--get','remote.origin.mirror') @(0,1)).Text -eq 'true') { throw 'Mirror push is refused.' }
    $trackingRemote = (Invoke-PublishGit $root @('config','--get',('branch.'+$Target.Branch+'.remote')) @(0,1)).Text
    $trackingRef = (Invoke-PublishGit $root @('config','--get',('branch.'+$Target.Branch+'.merge')) @(0,1)).Text
    if (($trackingRemote -and $trackingRemote -cne 'origin') -or ($trackingRef -and $trackingRef -cne ('refs/heads/'+$Target.Branch))) { throw 'Unexpected upstream.' }
    foreach ($name in @('index.lock','MERGE_HEAD','CHERRY_PICK_HEAD','REVERT_HEAD','rebase-merge','rebase-apply')) {
        if (Test-Path -LiteralPath (Join-Path $root ('.git/'+$name))) { throw "Active Git operation: $name" }
    }
    if ((Invoke-PublishGit $root @('diff','--cached','--name-only')).Text) { throw 'Existing staged changes require separate review.' }
    foreach ($file in $Target.IdentityFiles) {
        [void](Assert-PublishPath $root $file)
        [void](Invoke-PublishGit $root @('cat-file','-e',('HEAD:'+$file)))
    }
}

function Get-PublishExclusion([string]$Path, [bool]$Tracked, $DeliveryFiles) {
    if ($Path -match '^(?i:AGENTS(?:_for_Codex)?|CLAUDE|WORKSPACE_RULES|HANDOFF|CODEX_HANDOFF|SELF_CHECK|MANUAL_CLEANUP|DEVELOPMENT_CHECKLIST)\.md$' -or
        $Path -match '^docs/(internal|development|verification|pdf_extracts)/' -or
        $Path -match '^docs/(PMBUS_1_3_1_PARTS_GAP_AUDIT|PMBUS_SPEC_SEMANTIC_GAP_AUDIT|PMBUS_PLACEHOLDER_AUDIT|PMBUS_CORSAIR_PROFILE_REVIEW|SMBALERT_BEHAVIOR_REVIEW)\.md$') {
        return 'local engineering record / extracted or generated evidence'
    }
    if ($Path -match '(^|/)(\.git|\.vs|\.agents|\.codex|\.claude|\.aws|\.venv|venv|node_modules|__pycache__|captures|recordings|sessions|logs|private|tmp|artifacts)(/|$)' -or
        $Path -match '^firmware/build/' -or $Path -match '(^|/)(obj|verify|verify_obj|CMakeFiles|x64|Debug|Release)(/|$)') { return 'local/generated directory' }
    if ($Tracked -and $Path -in $DeliveryFiles) { return '' }
    if ($Path -match '^build/' -and $Path -notin @('build/.gitkeep','build/README.md')) { return 'unreviewed build output' }
    if ($Path -match '(?i)\.(obj|o|pch|idb|pdb|ilk|res|iobj|ipdb|tlog|lastbuildstate|pyc|pyo|db|sqlite|sqlite3|log|jsonl|dmp|bin|uf2|elf|hex|map|dis|exe|dll|lib|zip|7z)$') { return 'runtime/compiler/image/archive payload' }
    if ($Path -match '(^|/)(auth\.json|credentials[^/]*|secrets?\.[^/]+|\.env(?:\.[^/]+)?|github_push\.local\.psd1)$') { return 'personal configuration / credentials' }
    if ($Path -match '\.ini$' -and [IO.Path]::GetFileName($Path) -notin @('lang_en.ini','lang_tw.ini')) { return 'personal INI' }
    return ''
}

function Assert-PublishText([string]$Path, [string]$Text) {
    if ($Text -match '-----BEGIN (RSA |EC |OPENSSH )?PRIVATE KEY-----|ghp_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{40,}|sk-(proj-)?[A-Za-z0-9_-]{24,}') {
        throw "Possible credential; contents withheld: $Path"
    }
}

function Get-PublishSnapshot($Target) {
    Assert-PublishRepository $Target
    $root = $Target.Root
    $index = Get-PublishIndex $root
    $new = @((Invoke-PublishGit $root @('ls-files','--others','--exclude-standard','-z')).Text.Split([char]0) | Where-Object { $_ })
    $changed = @((Invoke-PublishGit $root @('diff','--no-renames','--name-only','-z','HEAD','--')).Text.Split([char]0) | Where-Object { $_ })
    $pending = @($changed+$new | Sort-Object -Unique)
    $scope = if ($Target.ContainsKey('ScopePaths')) { @($Target.ScopePaths) } else { $null }
    if ($null -ne $scope) { $pending = @($pending | Where-Object { $_ -in $scope }) }
    $entries = @{}; $removals = @(); $withheld = @()
    $inspect = if ($null -ne $scope) { @($scope | Where-Object { $index.ContainsKey($_) -or $_ -in $new }) } else { @(@($index.Keys)+$new | Sort-Object -Unique) }
    foreach ($path in $inspect) {
        if (Test-PublishHostWithheld $Target $path) { $withheld += @{Path=$path;Reason='firmware outside the host-tool publication scope'}; continue }
        $full = Assert-PublishPath $root $path
        $reason = Get-PublishExclusion $path ($index.ContainsKey($path)) $Target.DeliveryFiles
        if ($reason) {
            $withheld += @{Path=$path;Reason=$reason}
            if ($index.ContainsKey($path)) { $removals += $path }
            continue
        }
        if (!(Test-Path -LiteralPath $full -PathType Leaf)) { continue }
        if ((Get-Item -LiteralPath $full).Length -gt 50MB) { throw "File exceeds 50 MiB publication limit: $path" }
        if ($path -match '\.(c|h|cpp|hpp|ps1|psd1|bat|json|md|txt|ini|rc|yml|yaml|toml|csv)$') {
            Assert-PublishText $path ([IO.File]::ReadAllText($full))
        }
    }
    foreach ($path in $pending) {
        if (Test-PublishHostWithheld $Target $path) { continue }
        if (Get-PublishExclusion $path ($index.ContainsKey($path)) $Target.DeliveryFiles) { continue }
        $full = Assert-PublishPath $root $path
        if (Test-Path -LiteralPath $full -PathType Leaf) {
            $entries[$path] = @{Hash=(Get-FileHash -LiteralPath $full -Algorithm SHA256).Hash;
                Blob=(Invoke-PublishGit $root @('hash-object',('--path='+$path),'--',$full)).Text;
                Mode=$(if($index.ContainsKey($path)){$index[$path].Mode}else{'100644'})}
        } else { $entries[$path] = @{Hash='deleted';Blob='';Mode=''} }
    }
    $head = (Invoke-PublishGit $root @('rev-parse','HEAD')).Text
    $fingerprint = $head+'|'+(($entries.Keys | Sort-Object | ForEach-Object { $_+':'+$entries[$_].Hash+':'+$entries[$_].Blob }) -join '|')+'|'+(($removals | Sort-Object) -join '|')
    return @{Head=$head;Index=$index;Entries=$entries;Removals=$removals;Withheld=$withheld;Fingerprint=$fingerprint}
}

function Get-LivePublishRemote($Target) {
    $metadata = (Invoke-PublishProcess gh @('repo','view',$Target.Repository,'--json','nameWithOwner,visibility,isArchived')).Text | ConvertFrom-Json
    $expected=Get-PublishExpectedVisibility $Target
    if ($metadata.nameWithOwner -cne $Target.Repository -or $metadata.visibility -cne $expected -or $metadata.isArchived) {
        throw 'The pinned repository identity/visibility must match and remain active.'
    }
    $refs = (Invoke-PublishGit $Target.Root @('ls-remote','--refs','origin')).Text
    $match = [regex]::Match($refs,'(?m)^([0-9a-f]{40,64})\s+refs/heads/'+[regex]::Escape($Target.Branch)+'\r?$')
    if (!$match.Success -and $refs) { throw 'Remote is nonempty but the expected branch is missing.' }
    return @{Repository=$metadata.nameWithOwner;Visibility=$metadata.visibility;Private=($metadata.visibility -ceq 'PRIVATE');Sha=$match.Groups[1].Value}
}

function Assert-PublishRemote($Target, $Remote) {
    if ($Remote.Repository -cne $Target.Repository -or (Get-PublishObservedVisibility $Remote) -cne (Get-PublishExpectedVisibility $Target)) { throw 'Pinned repository identity/visibility not confirmed.' }
    if (!$Remote.Sha) { return }
    if ($Remote.Sha -notmatch '^[0-9a-f]{40,64}$') { throw 'Invalid remote commit identity.' }
    $known = Invoke-PublishGit $Target.Root @('cat-file','-e',($Remote.Sha+'^{commit}')) @(0,128)
    if ($known.Code) { throw 'Remote commit is not available locally. Fetch origin explicitly, then retry; preview never fetches.' }
    if ((Invoke-PublishGit $Target.Root @('merge-base','--is-ancestor',$Remote.Sha,'HEAD') @(0,1)).Code) { throw 'Remote is ahead/diverged; reconcile explicitly.' }
    foreach ($file in $Target.IdentityFiles) { [void](Invoke-PublishGit $Target.Root @('cat-file','-e',($Remote.Sha+':'+$file))) }
    $readme = (Invoke-PublishGit $Target.Root @('show',($Remote.Sha+':README.md'))).Text
    if ($readme -notmatch ('(?m)^# '+[regex]::Escape($Target.Title)+'(?:[ (]|$)')) { throw 'Remote README identity mismatch.' }
}

function New-PublishPlan($Target, [switch]$LocalOnly, [scriptblock]$Probe = {param($t) Get-LivePublishRemote $t}) {
    $snapshot = Get-PublishSnapshot $Target
    $hook = Join-Path $Target.Root 'scripts/verify_publish.ps1'
    if (Test-Path -LiteralPath $hook) {
        [void](Assert-PublishPath $Target.Root 'scripts/verify_publish.ps1')
        [void](Invoke-PublishProcess powershell.exe @('-NoProfile','-ExecutionPolicy','Bypass','-File',$hook))
        if ((Get-PublishSnapshot $Target).Fingerprint -cne $snapshot.Fingerprint) { throw 'Publication hook changed the preview.' }
    }
    $remote = $null
    if (!$LocalOnly) { $remote = & $Probe $Target; Assert-PublishRemote $Target $remote }
    return @{Target=$Target;Snapshot=$snapshot;Remote=$remote;LocalOnly=[bool]$LocalOnly}
}

function Show-PublishPlan($Plan) {
    Write-Host "Repository: $($Plan.Target.Repository), pinned $(Get-PublishExpectedVisibility $Plan.Target), branch $($Plan.Target.Branch)"
    if ($Plan.LocalOnly) { Write-Host 'LOCAL ONLY: remote identity/visibility/ancestry NOT VERIFIED. No Git writes.' }
    else { Write-Host "Remote: $(if($Plan.Remote.Sha){$Plan.Remote.Sha}else{'verified empty repository'})" }
    foreach ($path in $Plan.Snapshot.Entries.Keys | Sort-Object) { Write-Host "UPDATE $path" }
    foreach ($path in $Plan.Snapshot.Removals | Sort-Object) { Write-Host "UNTRACK (keep local file) $path" }
    foreach ($item in $Plan.Snapshot.Withheld | Where-Object {$_.Path -notin $Plan.Snapshot.Removals}) { Write-Host "EXCLUDE $($item.Path): $($item.Reason)" }
    Write-Host 'History is preserved. Host publication does not grant firmware board acceptance.'
}

function Assert-PublishOutgoing($Target, [string]$RemoteSha) {
    $range = @('rev-list','HEAD')
    if ($RemoteSha) { $range += ('^'+$RemoteSha) }
    foreach ($commit in @((Invoke-PublishGit $Target.Root $range).Text -split '\r?\n' | Where-Object {$_})) {
        $paths = (Invoke-PublishGit $Target.Root @('diff-tree','--root','-m','--no-commit-id','--no-renames','--diff-filter=ACMRT','--name-only','-r','-z',$commit)).Text
        foreach ($path in $paths.Split([char]0) | Where-Object {$_}) {
            if (Test-PublishHostWithheld $Target $path) { throw "Outgoing history changes firmware outside the host scope: $path" }
            if (Get-PublishExclusion $path $true $Target.DeliveryFiles) { throw "Outgoing history contains excluded content; no history rewritten: $path" }
            if ($path -match '\.(c|h|cpp|hpp|ps1|psd1|bat|json|md|txt|ini|rc|yml|yaml|toml|csv)$') {
                Assert-PublishText $path (Invoke-PublishGit $Target.Root @('show',($commit+':'+$path))).Text
            }
        }
    }
}

function Invoke-PublishPlan($Plan, [string]$Message, [scriptblock]$Probe = {param($t) Get-LivePublishRemote $t}) {
    if ($Plan.LocalOnly) { throw 'A local-only preview cannot authorize publication.' }
    if ([string]::IsNullOrWhiteSpace($Message)) { throw 'Commit message is required.' }
    $target = $Plan.Target; $root = $target.Root; $snapshot = $Plan.Snapshot
    $now = New-PublishPlan $target -Probe $Probe
    if ($now.Snapshot.Fingerprint -cne $snapshot.Fingerprint -or $now.Remote.Sha -cne $Plan.Remote.Sha) { throw 'Worktree or remote changed after preview; nothing staged.' }
    Assert-PublishOutgoing $target $Plan.Remote.Sha
    if ($snapshot.Entries.Count -or $snapshot.Removals.Count) {
        foreach ($key in @('user.name','user.email')) {
            if (!(Invoke-PublishGit $root @('config','--get',$key) @(0,1)).Text) { throw "Missing Git $key" }
        }
        # Exact reviewed paths only; never git add-all or delete working files.
        foreach ($path in $snapshot.Removals) { [void](Invoke-PublishGit $root @('rm','--cached','--ignore-unmatch','--',$path)) }
        foreach ($path in $snapshot.Entries.Keys) { [void](Invoke-PublishGit $root @('add','--',$path)) }
        $expected = @{}
        foreach ($path in $snapshot.Index.Keys) { if ($path -notin $snapshot.Removals) { $expected[$path]=$snapshot.Index[$path] } }
        foreach ($path in $snapshot.Entries.Keys) {
            $entry=$snapshot.Entries[$path]
            if ($entry.Hash -eq 'deleted') { $expected.Remove($path) }
            else { $expected[$path]=@{Mode=$entry.Mode;Sha=$entry.Blob} }
        }
        $actual = Get-PublishIndex $root
        if ($actual.Count -ne $expected.Count) { throw 'Unexpected index paths; index retained for review.' }
        foreach ($path in $expected.Keys) {
            if (!$actual.ContainsKey($path) -or $actual[$path].Sha -cne $expected[$path].Sha -or $actual[$path].Mode -cne $expected[$path].Mode) {
                throw "Staged content differs from preview; index retained: $path"
            }
        }
        $tree = (Invoke-PublishGit $root @('write-tree')).Text
        [void](Invoke-PublishGit $root @('commit','-m',$Message))
        if ((Invoke-PublishGit $root @('rev-parse','HEAD^{tree}')).Text -cne $tree -or (Invoke-PublishGit $root @('rev-parse','HEAD^')).Text -cne $snapshot.Head) {
            throw 'Commit hook/concurrent change altered the approved commit; no push attempted.'
        }
    }
    $head = (Invoke-PublishGit $root @('rev-parse','HEAD')).Text
    $after = Get-PublishSnapshot $target
    if ($after.Entries.Count -or $after.Removals.Count -or $after.Head -cne $head) { throw 'Repository changed during commit; no push attempted.' }
    $remote = & $Probe $target
    Assert-PublishRemote $target $remote
    if ($remote.Sha -cne $Plan.Remote.Sha) { throw 'Remote changed before push; local commit retained.' }
    Assert-PublishOutgoing $target $remote.Sha
    if ($head -ceq $remote.Sha) { Write-Output "UNCHANGED: $head"; return }
    [void](Invoke-PublishGit $root @('push','--porcelain','--no-force','--no-follow-tags','--recurse-submodules=no','origin',($head+':refs/heads/'+$target.Branch)))
    $verified = & $Probe $target
    if ((Get-PublishObservedVisibility $verified) -cne (Get-PublishExpectedVisibility $target) -or $verified.Repository -cne $target.Repository -or $verified.Sha -cne $head) {
        throw 'Push returned but pinned visibility/SHA verification failed; inspect remote before retry.'
    }
    Write-Output "PUSH VERIFIED: $($target.Repository) $head"
}
