# PiPico_Drummer
A simple loop drum machine for the Raspberry Pi Pico. Plays drum loops in full "CD Quality" 16-bit stereo audio.

Samples are generated and placed into a ring buffer to be played. The sample rate is about 44100.

## Why?
I wanted an excuse to experiment with the Pico's PIO progammable interface hardware. And everybody loves a drum machine.

## Hardare 
This uses the Pioroni Pico Audio Pack https://shop.pimoroni.com/products/pico-audio-pack

It should be able to be adopted to any other I2S audio pack, but you may need to change the order of bits in the "side-set" values in the pio_i2s.pio hardware driver.

## Building
If you have the Pico SDK installed, and the encironment correct you should just be able to type:

    cmake CMakeLists.txt
    make

And then upload the resulting image to your Pico

## Licensing
This project is released under the MIT license. It is just a hack so enjoy.

Parts of the hardware specific DMA code are from adapted from the Raspberry Pi Pico examples repo: https://github.com/raspberrypi/pico-examples 

Those parts of the code are "Copyright 2020 (c) 2020 Raspberry Pi (Trading) Ltd.", but what is the
point of example code if you can't use and adapt them into your own projects?

## Say Hi!
If you have fun with, please drop me an email to say "Hi!". If you do something awesome based
on this the I also want to know about it!
