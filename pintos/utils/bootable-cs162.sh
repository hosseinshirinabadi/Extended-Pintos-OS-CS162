#!/usr/bin/env bash

BUILD=.
EXAMPLES=../../examples
OUTPUT=cs162proj.dsk

LOADER=$BUILD/loader.bin
KERNEL=$BUILD/kernel.bin

ARGS="--kernel=$KERNEL --loader $LOADER"
echo $ARGS

make -C $EXAMPLES

rm -f $OUTPUT
pintos -v -k --make-disk $OUTPUT --kernel=$KERNEL --loader $LOADER --qemu --filesys-size=4 --align=full -- -q  -f

PREFIX=
if [ "$1" != "--simple" ]
then
	PREFIX=/bin/
	pintos -v -k --qemu --disk $OUTPUT -p $EXAMPLES/mkdir -a mkdir -- -q run "mkdir bin"
	pintos -v -k --qemu --disk $OUTPUT -p $EXAMPLES/rm -a rm -- -q run "rm mkdir rm"
fi

COPY_FILES_CMD="pintos -v -k --qemu --disk $OUTPUT"
for file in $(ls $EXAMPLES)
do
	if [ -f $EXAMPLES/$file ] && [ -x $EXAMPLES/$file ]
	then
		 COPY_FILES_CMD+=" -p $EXAMPLES/$file -a $PREFIX$file"
	fi
done

# Add some regular files too
echo -e "I need to study for the CS 162 final. Do you?" > file1.txt
echo -e "I need to study for the CS 186 final. Do you?" > file2.txt
COPY_FILES_CMD+=" -p ./file1.txt -a file1.txt"
COPY_FILES_CMD+=" -p ./file2.txt -a file2.txt"

COPY_FILES_CMD+=" -- -q"

eval $COPY_FILES_CMD
rm -f file1.txt file2.txt

# This last part is necessary when booting in VMWare or on real hardware
pintos-set-cmdline $OUTPUT -- -q run ${PREFIX}shell
