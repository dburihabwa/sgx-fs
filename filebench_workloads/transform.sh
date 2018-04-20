#! /usr/bin/env bash


transform() {
    local files=$(find -type f -iname "*.f" | xargs)
    for f in ${files[@]}; do
        echo "- ${f}"
        sed -i "s/,cached=\$cached//g" "${f}"
        if [[ $(grep --count "/tmp/mnt/fuse" "${f}") -eq "0" ]]; then
            sed -i "s|/tmp|tmp/mnt/fuse|g" "${f}"
        fi
        if [[ $(grep --count "finishoncount" "${f}") -ge "1" ]]; then
            if [[ $(grep --count "run" "${f}") -eq "0" ]]; then
                echo "run" >> "${f}"
            fi
        elif [[ $(grep --count "run" "${f}") -eq "0" ]]; then
                echo "run 60" >> "${f}"
        fi
    done
}

transform
