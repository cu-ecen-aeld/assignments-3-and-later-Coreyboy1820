#!/bin/bash

help() {
    echo "type --help for this information"
    echo "Expected usage is writer.sh file_path text_to_write"
}

errorCheckWriterApp() {
    
    # number of arguments validation
    if [ $numberOfArgs -ne 2 ]
    then
        echo "Error, expected 2 arguments but got $numberOfArgs"
        help
        exit 1
    fi

    if [[ $1 -eq "--help" ]] 
    then
        help
        exit 0
    fi

}

writerApp() {
    # IFS says to split on the delimeter, read -r says to not tread \ as an escape
    # -a tells bash to store it into the array that follows
    IFS="/" read -r -a parts <<< "$writeFile"

    length=${#parts[@]}
    nameOfFile="${parts[$length-1]}"
    
    firstChar=${writeFile:0:1}
    nameOfDir=""

    
    # get all items except for the last one in the file path
    for (( i=0; i < length-1; i++ ))
    do
        nameOfDir+=${parts[i]}/
        # check to see if the directory exists, ignore all outputs from the command
        # if it doesn't exist, make the directory
        ls "${nameOfDir}" 2>/dev/null >> /dev/null 
        if [ "$?" != "0" ] 
        then
            mkdir $nameOfDir
        fi
    done

    touch $writeFile

    echo "$writeStr" > $writeFile

}

numberOfArgs=$#
writeFile=$1
writeStr=$2
errorCheckWriterApp

writerApp