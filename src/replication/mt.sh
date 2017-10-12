#!/bin/sh

#set -x

killall server
killall client
timeout=${timeout:-10}
bufsize=${bufsize:-4096}
nothreads=1

for nosessions in 100; do
for nothreads in 1 4; do
  echo "======================> (test2) Bufsize: $bufsize Threads: $nothreads Sessions: $nosessions"
  sleep 1
  ./server 127.0.0.1 33333 $nothreads $bufsize & srvpid=$!
  sleep 1
  ./client 127.0.0.1 33333 $nothreads $bufsize $nosessions $timeout
  sleep 1
  kill -9 $srvpid
  sleep 5
done
done
