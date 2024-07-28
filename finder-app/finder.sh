#!/bin/sh

# For assignment #1


# Check if 2 input arguments provided
if [ "$#" -ne 2 ]; then
    echo "Error: Must provide exactly two arguments."
    echo "Usage: $0 <filesdir> <searchstr>"
    exit 1
fi

#Assign the args to vars
filesdir="$1"
searchstr="$2"

# Check if filesdir is a directory on the filesystem
if [ ! -d "$filesdir" ]; then
    # -d flag checks if the given path is a directory on the filesystem
    echo "Error: Directory $filesdir does not exist in the filesystem."
    exit 1
fi

numfiles=$(find "$filesdir" -type f | wc -l)
nummatchinglines=$(grep -r "$searchstr" "$filesdir" | wc -l)


echo "The number of files are $numfiles and the number of matching lines are $nummatchinglines"