#!/bin/bash

# traditional system call return values-- used in an `if`, this will be true when returning 0. Very Odd.
contains () {
    # odd syntax here for passing array parameters: http://stackoverflow.com/questions/8082947/how-to-pass-an-array-to-a-bash-function
    local list=$1[@]
    local elem=$2

    # echo "list" ${!list}
    # echo "elem" $elem

    for i in "${!list}"
    do
        # echo "Checking to see if" "$i" "is the same as" "${elem}"
        if [ "$i" == "${elem}" ] ; then
            # echo "$i" "was the same as" "${elem}"
            return 0
        fi
    done

    # echo "Could not find element"
    return 1
}

MODULE="${1%/}"
if [ ! -d "$1" ]; then
	echo "$0: Please specify the project folder."
	exit 0
fi

SOURCE_EXT=("c" "cpp" "cxx")

SOURCES=""
for f in "$MODULE"/*; do
	if [ -f "$f" ]; then
		filename=$(basename -- "$f")
		ext="${filename##*.}"

		if contains SOURCE_EXT $ext; then
			SOURCES="$SOURCES $f"
		fi
	fi
done

if [ ${#SOURCES} == 0 ]; then
	echo "$0: No source file."
	exit 0
fi

echo "Built target $MODULE"
exec g++ -std=c++11 -shared -fPIC -Wall -O2 -Iinclude ${CXXFLAGS} -o "${MODULE}.so" ${SOURCES}
