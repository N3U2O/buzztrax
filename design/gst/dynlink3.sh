#!/bin/bash

gcc -Wall -g `pkg-config gstreamer-0.10 --cflags --libs` dynlink3.c -o dynlink3-0.10
if test $? -ne 0; then
  exit 1
fi
rm *.{dot,png}

trap "echo ' aborted ...'" SIGINT
GST_DEBUG="dynlink:2" GST_DEBUG_DUMP_DOT_DIR=$PWD ./dynlink3-0.10

for file in dyn*.dot; do echo $file; dot -Tpng $file -o${file/dot/png}; done
eog *.png
