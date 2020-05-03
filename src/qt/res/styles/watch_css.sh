#!/bin/bash
#little script to streamline editing the sass to qss
#use sass monitor to generate css in /css folder
#run this script to monitor css files and generate qss files in the style folder

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
MonitorFile1="$DIR/css/nubits.css"
MonitorFile2="$DIR/css/nushares.css"
CopyDestination1="$DIR/nubits.qss"
CopyDestination2="$DIR/nushares.qss"

#checks to see if we're using debian or osx version of stat command
if [[ "$OSTYPE" == "darwin"* ]]; then
  convertArg="-t"
else
  convertArg="-c"
fi


echo "monitor file: $MonitorFile1"
echo "monitor file 2: $MonitorFile2"
echo "copy destination: $CopyDestination1"
echo "copy destination: $CopyDestination2"
echo ""


Mod=`stat "$convertArg" "%Y" "$MonitorFile1"`
CheckMod="$Mod"

Mod2=`stat "$convertArg" "%Y" "$MonitorFile2"`
CheckMod2="$Mod2"

while [ 1 -eq 1 ] 
do

  if [ "$CheckMod" != "$Mod" ]; then

  cp "$MonitorFile1" "$CopyDestination1"

  Mod="$CheckMod"

  echo "Copied new file at $(date)"

  fi

  if [ "$CheckMod2" != "$Mod2" ]; then

  cp "$MonitorFile2" "$CopyDestination2"

  Mod2="$CheckMod2"

  echo "Copied new file 2 at $(date)"

  fi

sleep 1

CheckMod=`stat "$convertArg" "%Y" "$MonitorFile1"`
CheckMod2=`stat "$convertArg" "%Y" "$MonitorFile2"`

done

