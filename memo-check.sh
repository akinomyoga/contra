#!/bin/bash

file=memo.txt

echo 番号のついている行の数
awk 'done && /^\* .*\[#D[0-9]+\]/ {print;} /^  Done/ {done=1}' "$file" | wc -l
echo

result=$(awk 'done && /^\* .*/ && !/\[#D[0-9]+\]/ {print;} /^  Done/ {done=1}' "$file")
if [[ $result ]]; then
  echo 番号のついていない行
  echo "$result"
  echo
fi

result=$(grep -o '\[#D[0-9]\{1,\}\]' "$file" | sort | uniq -d)
if [[ $result ]]; then
  echo 重複する番号
  echo "$result"
  echo
fi

echo 参照一覧
awk '/#D[0-9]+/ && !/\[#D[0-9]+\]$/ {print "'"$file"':" NR ": " $0;}' "$file"
grc '#D[0-9]+'
echo
