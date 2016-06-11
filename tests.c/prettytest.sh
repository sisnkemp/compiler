#!/bin/sh

c=../lang.c/c_`uname -m`

rm -f AST.*
for i in [a-z]*.c
do
	$c -a $i
	$c -a AST.0000.start.$i
	cmp AST.0000.start.$i AST.0000.start.AST.0000.start.$i
	if [ $? -eq 1 ]
	then
		exit
	fi
done
