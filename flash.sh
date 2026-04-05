#!/bin/bash

if [ -z "$RPI" ]; then
    RPI="rp2040"
fi

echo Flashing for $RPI

sudo openocd -f interface/cmsis-dap.cfg -f target/${RPI}.cfg -c "adapter speed 5000" -c "program build/relay.elf verify reset exit"
# sudo openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg -c "adapter speed 5000" -c "program build/relay.elf verify reset exit"

