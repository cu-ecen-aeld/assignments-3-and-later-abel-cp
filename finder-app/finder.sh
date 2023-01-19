#1! /bin/sh

if test $# -lt 2
then
    if [ $# -lt 1 ]
    then
       echo "Mising both filedir and searchstr arguments"
    else
	echo "Missing searchstr "
    fi
    exit 1
fi

if [ -d $1 ] 
then
    x=$( ls $1 | wc -l )
    y=$( grep -rin $2 $1 | wc -l )
    echo "The number of files are $x and the number of matching lines are $y"
else
    echo "$1 is not a directory"
    exit 1
fi

