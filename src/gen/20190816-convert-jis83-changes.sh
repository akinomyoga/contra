#!/bin/bash

function filter {
  sed '
    s/\b0\b/<SP>/g  ; s/\b16\b/0/g ; s/\b32\b/@/g ; s/\b48\b/P/g  ; s/\b64\b/`/g ; s/\b80\b/p/g
    s/\b1\b/!/g     ; s/\b17\b/1/g ; s/\b33\b/A/g ; s/\b49\b/Q/g  ; s/\b65\b/a/g ; s/\b81\b/q/g
    s/\b2\b/"/g     ; s/\b18\b/2/g ; s/\b34\b/B/g ; s/\b50\b/R/g  ; s/\b66\b/b/g ; s/\b82\b/r/g
    s/\b3\b/#/g     ; s/\b19\b/3/g ; s/\b35\b/C/g ; s/\b51\b/S/g  ; s/\b67\b/c/g ; s/\b83\b/s/g
    s/\b4\b/$/g     ; s/\b20\b/4/g ; s/\b36\b/D/g ; s/\b52\b/T/g  ; s/\b68\b/d/g ; s/\b84\b/t/g
    s/\b5\b/%/g     ; s/\b21\b/5/g ; s/\b37\b/E/g ; s/\b53\b/U/g  ; s/\b69\b/e/g ; s/\b85\b/u/g
    s/\b6\b/&/g     ; s/\b22\b/6/g ; s/\b38\b/F/g ; s/\b54\b/V/g  ; s/\b70\b/f/g ; s/\b86\b/v/g
    s/\b7\b/'\''/g  ; s/\b23\b/7/g ; s/\b39\b/G/g ; s/\b55\b/W/g  ; s/\b71\b/g/g ; s/\b87\b/w/g
    s/\b8\b/(/g     ; s/\b24\b/8/g ; s/\b40\b/H/g ; s/\b56\b/X/g  ; s/\b72\b/h/g ; s/\b88\b/x/g
    s/\b9\b/)/g     ; s/\b25\b/9/g ; s/\b41\b/I/g ; s/\b57\b/Y/g  ; s/\b73\b/i/g ; s/\b89\b/y/g
    s/\b10\b/*/g    ; s/\b26\b/:/g ; s/\b42\b/J/g ; s/\b58\b/Z/g  ; s/\b74\b/j/g ; s/\b90\b/z/g
    s/\b11\b/+/g    ; s/\b27\b/;/g ; s/\b43\b/K/g ; s/\b59\b/[/g  ; s/\b75\b/k/g ; s/\b91\b/{/g
    s/\b12\b/,/g    ; s/\b28\b/</g ; s/\b44\b/L/g ; s/\b60\b/\\/g ; s/\b76\b/l/g ; s/\b92\b/|/g
    s/\b13\b/-/g    ; s/\b29\b/=/g ; s/\b45\b/M/g ; s/\b61\b/]/g  ; s/\b77\b/m/g ; s/\b93\b/}/g
    s/\b14\b/./g    ; s/\b30\b/>/g ; s/\b46\b/N/g ; s/\b62\b/^/g  ; s/\b78\b/n/g ; s/\b94\b/~/g
    s/\b15\b/\//g   ; s/\b31\b/?/g ; s/\b47\b/O/g ; s/\b63\b/_/g  ; s/\b79\b/o/g
  '
}
function filter-hex {
  sed '
    s/\b20\b/<SP>/g ; s/\b30\b/0/g ; s/\b40\b/@/g ; s/\b50\b/P/g  ; s/\b60\b/`/g ; s/\b70\b/p/g
    s/\b21\b/!/g    ; s/\b31\b/1/g ; s/\b41\b/A/g ; s/\b51\b/Q/g  ; s/\b61\b/a/g ; s/\b71\b/q/g
    s/\b22\b/"/g    ; s/\b32\b/2/g ; s/\b42\b/B/g ; s/\b52\b/R/g  ; s/\b62\b/b/g ; s/\b72\b/r/g
    s/\b23\b/#/g    ; s/\b33\b/3/g ; s/\b43\b/C/g ; s/\b53\b/S/g  ; s/\b63\b/c/g ; s/\b73\b/s/g
    s/\b24\b/$/g    ; s/\b34\b/4/g ; s/\b44\b/D/g ; s/\b54\b/T/g  ; s/\b64\b/d/g ; s/\b74\b/t/g
    s/\b25\b/%/g    ; s/\b35\b/5/g ; s/\b45\b/E/g ; s/\b55\b/U/g  ; s/\b65\b/e/g ; s/\b75\b/u/g
    s/\b26\b/&/g    ; s/\b36\b/6/g ; s/\b46\b/F/g ; s/\b56\b/V/g  ; s/\b66\b/f/g ; s/\b76\b/v/g
    s/\b27\b/'\''/g ; s/\b37\b/7/g ; s/\b47\b/G/g ; s/\b57\b/W/g  ; s/\b67\b/g/g ; s/\b77\b/w/g
    s/\b28\b/(/g    ; s/\b38\b/8/g ; s/\b48\b/H/g ; s/\b58\b/X/g  ; s/\b68\b/h/g ; s/\b78\b/x/g
    s/\b29\b/)/g    ; s/\b39\b/9/g ; s/\b49\b/I/g ; s/\b59\b/Y/g  ; s/\b69\b/i/g ; s/\b79\b/y/g
    s/\b2A\b/*/g    ; s/\b3A\b/:/g ; s/\b4A\b/J/g ; s/\b5A\b/Z/g  ; s/\b6A\b/j/g ; s/\b7A\b/z/g
    s/\b2B\b/+/g    ; s/\b3B\b/;/g ; s/\b4B\b/K/g ; s/\b5B\b/[/g  ; s/\b6B\b/k/g ; s/\b7B\b/{/g
    s/\b2C\b/,/g    ; s/\b3C\b/</g ; s/\b4C\b/L/g ; s/\b5C\b/\\/g ; s/\b6C\b/l/g ; s/\b7C\b/|/g
    s/\b2D\b/-/g    ; s/\b3D\b/=/g ; s/\b4D\b/M/g ; s/\b5D\b/]/g  ; s/\b6D\b/m/g ; s/\b7D\b/}/g
    s/\b2E\b/./g    ; s/\b3E\b/>/g ; s/\b4E\b/N/g ; s/\b5E\b/^/g  ; s/\b6E\b/n/g ; s/\b7E\b/~/g
    s/\b2F\b/\//g   ; s/\b3F\b/?/g ; s/\b4F\b/O/g ; s/\b5F\b/_/g  ; s/\b6F\b/o/g
  '
}

cat <<EOF | filter
16-19 鯵 82-45 鰺
18-9  鴬 82-84 鶯
19-34 蛎 73-58 蠣
19-41 撹 57-88 攪
19-86 竃 67-62 竈
20-35 潅 62-85 灌
20-50 諌 75-61 諫
23-59 頚 80-84 頸
25-60 砿 66-72 礦
28-41 蕊 73-2  蘂
31-57 靭 80-55 靱
33-8  賎 76-45 賤
36-59 壷 52-68 壺
37-55 砺 66-74 礪
37-78 梼 59-77 檮
37-83 涛 62-25 濤
38-86 迩 77-78 邇
39-72 蝿 74-4  蠅
41-16 桧 59-56 檜
43-89 侭 48-54 儘
44-89 薮 73-14 藪
47-22 篭 68-38 籠  
EOF

echo 字体変更と追加
cat <<EOF | filter
22-38 尭 84-1 堯
43-74 槙 84-2 槇
45-58 遥 84-3 遙
64-86 瑶 84-4 瑤
EOF

# https://www.asahi-net.or.jp/~ax2s-kmtn/ref/jis_restore.html
echo JIS補助漢字および拡張漢字で復活した字体
cat <<EOF | filter-hex
30 22  唖  啞
31 6B  焔  焰
32 2A  鴎  鷗
33 7A  噛  嚙
36 22  侠  俠
36 6D  躯  軀
37 52  繋  繫
38 34  鹸  鹼
39 6D  麹  麴
3C 48  屡  屢
3D 2B  繍  繡
3E 55  蒋  蔣
3E 5F  醤  醬
40 66  蝉  蟬
41 5F  掻  搔
41 69  痩  瘦
42 4D  騨  驒
43 3D  箪  簞
44 4F  掴  摑
45 36  填  塡
45 3F  顛  顚
45 78  祷  禱
46 42  涜  瀆
47 39  嚢  囊
48 2E  溌  潑
48 30  醗  醱
4B 4B  頬  頰
4C 4D  麺  麵
4D 69  莱  萊
4F 39  蝋  蠟
5A 39  攅  攢
EOF
