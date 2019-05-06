#!/bin/bash

# Original file list
#
# 2010-01-10 16:30:28 sample.txt
# 2010-01-10 16:50:53 contrans.old.cpp
# 2010-01-11 14:50:03 old.cpp
# 2010-01-11 15:19:19 compile.sh
# 2010-01-11 15:34:50 contrans.cpp
# 2010-01-11 17:28:09 piano.esc
# 2010-01-11 20:35:10 cls.h
# 2010-01-11 20:36:04 cls.cpp
# 2010-01-12 00:06:09 samp.cpp
# 2010-01-12 00:12:52 colors.esc
# 2010-01-12 00:24:47 escseq.h
# 2010-01-12 00:54:30 escseq.cpp
# 2010-01-12 01:39:44 piano.cpp
# 2010-03-19 22:01:38 contrans.man

git add sample.txt
mv contrans.old.cpp contrans.cpp
git add contrans.cpp
git commit -m '[initial] add contrans.cpp' --date='2010-01-10 16:30:28 '

mv old.cpp cls_old.cpp
git add cls_old.cpp
git commit -m 'add cls_old.cpp' --date='2010-01-11 15:19:19'

git add compile.sh
git commit -m 'add compile.sh' --date='2010-01-11 15:19:19'

mv contrans.new.cpp contrans.cpp
git add contrans.cpp
git commit -m 'update contrans.cpp' --date='2010-01-11 15:34:50'

git add piano.esc
git commit -m 'add piano.esc' --date='2010-01-11 17:28:09'

git add cls.h cls.cpp
git commit -m 'add cls.{h,cpp}' --date='2010-01-11 20:36:04'

git add samp.cpp
git commit -m 'add samp.cpp' --date='2010-01-12 00:06:09'

git add colors.esc
git commit -m 'add colors.esc' --date='2010-01-12 00:12:52'

git add escseq.h escseq.cpp
git commit -m 'add escseq.{h,cpp}' --date='2010-01-12 00:54:30'

git add piano.cpp
git commit --m 'add piano.cpp' --date='2010-01-12 01:39:44'

git add contrans.man
git commit -m 'add contrans.man' --date='2010-03-19 22:01:38'
