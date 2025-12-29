#!/bin/bash
#
sudo openocd -f interface/cmsis-dap.cfg -f target/rp2350.cfg -c "adapter speed 5000" > openocd.log 2>&1 &

sleep 1

gdb-multiarch -ex 'target remote localhost:3333' build/relay.elf 

jobs -p | xargs kill



