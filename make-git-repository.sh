#!/bin/bash

# 2013-05-13 07:36:32 cattr.h~
# 2013-05-14 16:32:07 cattr.h
# 2013-05-20 02:17:39 term_target.h~
# 2013-05-20 02:18:03 i1.cpp~
# 2013-05-20 02:18:30 term_ncurses.cpp~
# 2013-05-20 02:18:42 a.exe
# 2013-05-20 02:23:20 i1.cpp
# 2013-05-20 02:24:30 term_target.h
# 2013-05-20 02:41:14 term_ncurses.cpp

git init

mv a/cattr.h~ cattr.h
git add cattr.h
git commit -m 'contra1: [initial] add cattr.h' --date='2013-05-13 07:36:32'

mv a/cattr.h cattr.h
git add -u
git commit -m 'contra1: update cattr.h' --date='2013-05-14 16:32:07'

mv a/term_target.h~ term_target.h
mv a/i1.cpp~ i1.cpp
mv a/term_ncurses.cpp~ term_ncurses.cpp
git add term_target.h
git add i1.cpp
git add term_ncurses.cpp
git commit -m 'contra1: add i1.cpp, term_target.h, term_ncurses.cpp' --date='2013-05-20 02:18:30'

mv a/i1.cpp a/term_target.h ./
git add -u
git commit -m 'contra1: update i1.cpp, term_target.h' --date='2013-05-20 02:24:30'

mv a/term_ncurses.cpp ./
git add -u
git commit -m 'contra1: update term_ncurses.cpp' --date='2013-05-20 02:41:14'
