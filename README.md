# Building and running

The first thing you need to do is build and compile [apple2e-audio-transport](https://github.com/hausdorff/apple2e-audio-transport), which will allow you to transport the interpreter over the wire to the Apple IIe. It's a matter of running `make compile` and dumping the binary (which is called `transport` at this point) into the directory of this project.

Next you want to make sure to grab all the other (public, widely-available) source dependencies below.

Now the interesting part.

Hook up your Apple II e to the audio jack. Run `make send` on your home computer. You should see something like the following:

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

This program is waiting for you to press return to send the message. Don't press return yet.

The second line says `Load at: $x..$y`, where `$x` and `$y` are hex addresses. Now go to your Apple IIe and open the monitor program. Usually this is `call -151`, but it might depend on your system. (We're on ProDOS.) The Monitor prompt like this:

`*`

Type `$x..$yR` into your Apple IIe's Monitor prompt. This will tell the Apple IIe to load from the audio port.

It will wait until it receives an answer.

Now press enter on your home machine to transmit the data.

You might have to run `make run` and press enter again---I'm not sure why it does this, but transmitting the package again should work.

This should drop you into the prompt.


# Dependencies

* [apple2e-audio-transport](https://github.com/hausdorff/apple2e-audio-transport), a small library written specifically for this project. Sends the program over the audio jack to the Apple IIe.
* [sox](http://sox.sourceforge.net/), which we use to emit sound to the Apple IIe.
* [cpp](http://gcc.gnu.org/onlinedocs/cpp/), the C preprocessor; we write macros to make programming in the 6502 instruction set slightly easier.
* [xa](http://www.floodgap.com/retrotech/xa/)(aka xa65), assembler for the 6502 instruction set.

# LICENSE

Distributed under MIT, which basically means that if you should use this code for anything, you just have to keep a note saying we wrote the code. That said, God help you should you actually decide to use this code.