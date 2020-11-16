#!/bin/bash

INPUTS=$1
OUTPUTS=$2
MAXTHREADS=$3


for file in $INPUTS/*.txt;
do
    for ((thread=1; thread<=$MAXTHREADS; thread++));
        do 
            TESTNAME=$(echo $file| cut -d "/" -f 2)

            echo InputFile=$TESTNAME NumThreads=$thread

            ./tecnicofs $file $OUTPUTS/$TESTNAME-$thread.txt $thread | grep "TecnicoFS completed in"
        done
done