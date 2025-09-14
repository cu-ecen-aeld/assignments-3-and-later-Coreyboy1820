#!/bin/sh

help() {
    echo "type --help for this information"
    echo "Expected usage is finder.sh file_path text_to_look_for"
}

# the purpose of this file is to take in a file path and a string (in that order)
# to look for at the given file path
finderApp() {
    numberOfFiles=$(find $filesDir/* -type f | wc -l)
    numberOfFilesWithString=$(grep $searchStr $filesDir/* | wc -l)
    echo "The number of files are $numberOfFiles and the number of matching lines are $numberOfFilesWithString" 
}

errorCheckFinderApp() {
    # number of arguments validation
    if [ $numberOfArgs -ne 2 ]
    then
        echo "Error, expected 2 arguments but got $numberOfArgs"
        help
        exit 1
    fi

    # File directory validation
    if [ ! -d "$filesDir" ]
    then
        echo "File path was invalid:" $filesDir
        help
        exit 1
    fi

    if [[ $1 -eq "--help" ]] 
    then
        help
        exit 0
    fi

}

numberOfArgs=$#
filesDir=$1
searchStr=$2
errorCheckFinderApp

finderApp