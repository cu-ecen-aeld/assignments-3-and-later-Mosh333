#!/bin/sh

# For assignment #1


# Check if 2 input arguments provided
if [ "$#" -ne 2 ]; then
    echo "Error: Must provide exactly two arguments."
    echo "Usage: $0 <writefile> <writestr>"
    exit 1
fi

# Assign the args to vars
writefile="$1"
writestr="$2"

# Get the dir path from writefile
dir=$(dirname "$writefile")

# Create the directory if it doesn't exist
mkdir -p "$dir"
mkdir_status=$?
if [ $mkdir_status -ne 0 ]; then
    echo "Error: Could not create directory $dir, exit status $mkdir_status"
    exit 1
fi

# Check directory permissions
ls -ld "$dir"

# Create the file if it doesn't exist using '>' instead of 'touch'
> "$writefile"
touch_status=$?
if [ $touch_status -ne 0 ]; then
    echo "Error: Could not create file $writefile, exit status $touch_status"
    echo "Debug: Checking file and directory status..."
    ls -ld "$dir"
    ls -l "$writefile"
    exit 1
fi

# Write the content to the file, overwriting any existing file
echo "$writestr" > "$writefile"
echo_status=$?
if [ $echo_status -ne 0 ]; then
    echo "Error: Could not write to file $writefile, exit status $echo_status"
    exit 1
fi

echo "File $writefile created successfully with content: $writestr"