$CutoffDate = 1770220800
$AuthorNames = @("lebaneseboii", "lebonho2002")

$files = git ls-files | Where-Object { $_ -match "\.(cpp|hpp|h|c|inl)$" -and $_ -notmatch "ffmpeg" -and $_ -notmatch "(?i)ImGuiFileDialog" }

$Results = @()

foreach ($f in $files) {
    $blame = git blame --line-porcelain $f 2>$null
    if ($null -eq $blame) { continue }

    $currentSha = ""
    $currentAuthor = ""
    $currentTime = 0
    $count = 0
    $commits = @{}

    foreach ($line in $blame) {
        if ($line -match "^([0-9a-f]{40})\s") {
            $currentSha = $matches[1]
        }

        if ($line.StartsWith("author ")) {
            $currentAuthor = $line.Substring(7).Trim()
        } elseif ($line.StartsWith("author-time ")) {
            $currentTime = [int]$line.Substring(12).Trim()
        } elseif ($line.StartsWith("`t")) {
            $code = $line.Substring(1).Trim()
            
            $isMatch = $false
            foreach ($a in $AuthorNames) {
                if ($currentAuthor -match $a) { $isMatch = $true; break }
            }
            
            if ($isMatch -and $currentTime -ge $CutoffDate) {
                if ($code -ne "" -and $code -ne "{" -and $code -ne "}" -and $code -ne "};" `
                    -and -not $code.StartsWith("//") -and -not $code.StartsWith("/*") `
                    -and -not $code.StartsWith("*") -and -not $code.EndsWith("*/")) {
                    $count++
                    $commits[$currentSha] = $true
                }
            }
        }
    }
    
    if ($count -gt 0) {
        $msgs = @()
        foreach ($sha in $commits.Keys) {
            $msg = git show -s --format=%s $sha
            $msg = $msg.Trim()
            if ($msgs -notcontains $msg) { $msgs += $msg }
        }
        
        $desc = $msgs -join ", "
        
        $obj = [PSCustomObject] @{
            File = $f
            Lines = $count
            Description = $desc
        }
        $Results += $obj
    }
}

$outputStr = "## Contributions Since Feb 6th, 2026 (Excluding Logic Libraries like ImGuiFileDialog and FFmpeg)`n`n"
foreach ($r in $Results) {
    $outputStr += "**File:** $($r.File)`n"
    $outputStr += "**Lines Edited:** $($r.Lines)`n"
    $outputStr += "**Work Description:** $($r.Description)`n"
    $outputStr += "---`n"
}
$outputStr | Out-File -FilePath report.txt -Encoding utf8
"Done."
