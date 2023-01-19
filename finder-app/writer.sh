#1! /bin/sh

if test $# -lt 2
then
    if [ $# -lt 1 ]
    then
       echo "Mising both wrtiefile and writestr arguments"
    else
	echo "Missing writestr "
    fi
    exit 1
fi

DIRNAME=$( dirname $1 )
FILENAME=$( basename $1 )
if [ -e $DIRNAME ] 
then
    echo $2 > $1
else
    mkdir -p $DIRNAME
    if [ $? -gt 0 ]
    then
	echo "can not create file $1"
	exit 1
    else
	echo $2 > $1
    fi
fi

