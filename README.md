# Building and running

The first thing you need to do is build and compile [apple2e-audio-transport](https://github.com/hausdorff/apple2e-audio-transport), which will allow you to transport the interpreter over the wire to the Apple IIe. It's a matter of running `make compile` and dumping the binary (which is called `transport` at this point) into the directory of this project.

Next you want to make sure to grab all the other (public, widely-available) source dependencies below.

Now the interesting part.

Hook up your Apple II e to the audio jack. Type `0800..08D2R` (or whatever the last address of your program is) and run `make send`. You should see something like the following:

```
$ make send
Length: 351
Load at: 0800..095e
Press enter when ready...
-: (raw)

  Encoding: Signed PCM
  Channels: 1 @ 32-bit
Samplerate: 44100Hz
Replaygain: off
  Duration: unknown

In:0.00% 00:00:00.00 [00:00:00.00] Out:0     [      |      ]        Clip:0
```

Just press enter. It will transmit the signal.

This should drop you into the prompt, on your Apple 2 e, or it will drop you into the prompt after we're done writing the interpreter.


# Dependencies

* [apple2e-audio-transport](https://github.com/hausdorff/apple2e-audio-transport), a small library written specifically for this project. Sends the program over the audio jack to the Apple IIe.
* [sox](http://sox.sourceforge.net/), which we use to emit sound to the Apple IIe.
* [cpp](http://gcc.gnu.org/onlinedocs/cpp/), the C preprocessor; we write macros to make programming in the 6502 instruction set slightly easier.
* [xa](http://www.floodgap.com/retrotech/xa/)(aka xa65), assembler for the 6502 instruction set.

# LICENSE

Distributed under MIT, which basically means that if you should use this code for anything, you just have to keep a note saying we wrote the code. That said, God help you should you actually decide to use this code.