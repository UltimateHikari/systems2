#!/bin/bash
for number in {1..510}
do
curl localhost:8080 > /dev/null
echo $number
done
exit 0
