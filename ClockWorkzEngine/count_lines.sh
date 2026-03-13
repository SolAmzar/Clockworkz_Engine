#!/bin/bash

# Target timestamp for 2026-02-06 00:00:00 +0800
CUTOFF=1770220800
AUTHOR1="lebaneseboii"
AUTHOR2="lebonho2002"

declare -A file_counts
declare -A file_commits

# get all tracked cpp/h files
git ls-files | grep -E '\.(cpp|hpp|h|c|inl)$' | while read -r FILE; do
    # Read blame output
    git blame --line-porcelain "$FILE" 2>/dev/null > /tmp/blame_tmp || continue
    
    current_sha=""
    current_author=""
    current_time=0
    
    while IFS= read -r line; do
        if [[ $line =~ ^([0-9a-f]{40})\  ]]; then
            current_sha="${BASH_REMATCH[1]}"
            current_author=""
            current_time=0
            # Next lines define properties until \t
        elif [[ $line == author\ * ]]; then
            current_author="${line#author }"
        elif [[ $line == author-time\ * ]]; then
            current_time="${line#author-time }"
        elif [[ "$line" == $'\t'* ]]; then
            # The actual code line starts with tab
            code="${line:1}"
            
            # Check author and time
            # we need case insensitive match
            author_lower=$(echo "$current_author" | tr '[:upper:]' '[:lower:]')
            if [[ "$author_lower" == *"$AUTHOR1"* ]] || [[ "$author_lower" == *"$AUTHOR2"* ]]; then
                if [ "$current_time" -ge "$CUTOFF" ]; then
                    # Check if code is a comment or blank
                    # strip whitespace
                    code_stripped=$(echo "$code" | sed 's/^[ \t]*//;s/[ \t]*$//')
                    
                    if [ -z "$code_stripped" ]; then
                        continue
                    fi
                    
                    if [ "$code_stripped" = "{" ] || [ "$code_stripped" = "}" ] || [ "$code_stripped" = "};" ]; then
                        continue
                    fi
                    
                    if [[ "$code_stripped" == //* ]] || [[ "$code_stripped" == /* ]] || [[ "$code_stripped" == \** ]] || [[ "$code_stripped" == */* ]]; then
                        continue
                    fi
                    
                    # It's a valid code line!
                    echo "$FILE:$current_sha" >> /tmp/valid_lines
                fi
            fi
        fi
    done < /tmp/blame_tmp
done

# aggregate results
if [ -f /tmp/valid_lines ]; then
    cat /tmp/valid_lines | awk -F':' '{
        file=$1
        sha=$2
        count[file]++
        commits[file] = commits[file] " " sha
    }
    END {
        for (f in count) {
            print f "|" count[f] "|" commits[f]
        }
    }' > /tmp/aggregated
    
    while IFS='|' read -r file count shas; do
        echo "## File: $file"
        echo "Lines Edited: $count"
        
        # get unique commits messages
        msg_list=""
        for sha in $shas; do
            msg=$(git show -s --format=%s $sha)
            if [[ "$msg_list" != *"$msg"* ]]; then
                msg_list="$msg_list, $msg"
            fi
        done
        msg_list=${msg_list#,} # remove leading comma
        
        echo "Work Description: $msg_list"
        echo "---"
    done < /tmp/aggregated
else
    echo "No matching lines found."
fi
