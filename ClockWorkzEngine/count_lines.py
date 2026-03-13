import os
import subprocess
import datetime
import re

# We want lines from Feb 6th, 2026 onwards
# Timezone is +0800 as per git log, so let's use a unix timestamp for Feb 6, 2026 00:00:00 +0800
# 2026-02-06 00:00:00 +0800 is 1770220800
cutoff_timestamp = 1770220800

author_names = ["lebaneseboii", "lebonho2002"]

def is_code_line(line):
    line = line.strip()
    if not line: return False
    # Exclude common braces
    if line == "{" or line == "}" or line == "};": return False
    # Exclude comments
    if line.startswith("//"): return False
    if line.startswith("/*") or line.startswith("*") or line.endswith("*/"): return False
    # Exclude obvious headers/pragmas if considered "titles", but maybe keep includes? The user said "titles", which usually means big block comment headers. Let's just exclude block comments.
    return True

def get_tracked_files():
    result = subprocess.run(["git", "ls-files"], capture_output=True, text=True, encoding='utf-8', errors='replace')
    files = result.stdout.splitlines()
    # Filter C++ related files
    return [f for f in files if f.endswith(('.cpp', '.hpp', '.h', '.c', '.inl'))]

files = get_tracked_files()

file_stats = {}
# file_stats[file] = {"count": 0, "commits": set()}

commit_messages = {}

def get_commit_msg(sha):
    if sha in commit_messages: return commit_messages[sha]
    res = subprocess.run(["git", "show", "-s", "--format=%s", sha], capture_output=True, text=True, encoding='utf-8', errors='replace')
    msg = res.stdout.strip()
    commit_messages[sha] = msg
    return msg

for file in files:
    try:
        res = subprocess.run(["git", "blame", "--line-porcelain", file], capture_output=True, text=True, encoding='utf-8', errors='replace')
    except Exception as e:
        continue
    
    lines = res.stdout.splitlines()
    
    i = 0
    while i < len(lines):
        # A porcelain blame entry starts with:
        # <sha> <original_line_num> <final_line_num> <group_lines>
        parts = lines[i].split(' ')
        sha = parts[0]
        i += 1
        
        author = ""
        author_time = 0
        line_content = ""
        
        while i < len(lines):
            if lines[i].startswith('\t'):
                line_content = lines[i][1:]
                i += 1
                break
            elif lines[i].startswith("author "):
                author = lines[i][len("author "):].strip()
            elif lines[i].startswith("author-time "):
                author_time = int(lines[i][len("author-time "):].strip())
            i += 1
            
        # Check if match
        is_user = any(a.lower() in author.lower() for a in author_names)
        if is_user and author_time >= cutoff_timestamp:
            if is_code_line(line_content):
                if file not in file_stats:
                    file_stats[file] = {"count": 0, "commits": set()}
                file_stats[file]["count"] += 1
                file_stats[file]["commits"].add(sha)

# Output formatting
print("## Contributions Since Feb 6th, 2026\n")
for file, stats in sorted(file_stats.items(), key=lambda x: x[0]):
    print(f"**File:** `{file}`")
    print(f"**Lines Added/Edited:** {stats['count']}")
    
    # Get descriptions
    msgs = set()
    for sha in stats["commits"]:
        msgs.add(get_commit_msg(sha))
    
    print(f"**Work Description:** {', '.join(msgs)}")
    print("---")
