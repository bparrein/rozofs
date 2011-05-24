#!/bin/bash
indent -kr -l78 --no-tabs -bs -nbbo -il0 -nhnl -brf ../src/*.h
indent -kr -l78 --no-tabs -bs -nbbo -il0 -nhnl -brf ../src/*.c
indent -kr -l78 --no-tabs -bs -nbbo -il0 -nhnl -brf ../tests/*.c

rm -f ../src/*.?~
rm -f ../tests/*.?~
