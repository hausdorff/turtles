# Building and running

The first thing you need to do is build and compile [apple2e-audio-transport](https://github.com/hausdorff/apple2e-audio-transport) (itself largely derived from a core part of [ADTPro](http://adtpro.cvs.sourceforge.net/)), which will allow you to transport the interpreter over the wire to the Apple IIe. It's a matter of running `make compile` and dumping the binary (which is called `transport` at this point) into the directory of this project.

Next you want to make sure to grab all the other (public, widely-available) source dependencies below.

Now the interesting part.

> **TEMPORARY NOTE:** We have developed a full prototype in C. The file is `a2lisp.c`, and you can compile and send this across the wire. The eventual target is 6502 assembly, which we are in the process of porting now.

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

You might have to run `make send` and press enter again---I'm not sure why it does this, but transmitting the package again should work.

This should drop you into the prompt.

# Examples
Below we provide some examples of what you can do with the current prototype.

## Atoms and primitives
Numbers evaluate to themselves:

    > 42
    42

Basic arithmetic is provided by the built-in primitives `PLUS`, `MINUS`,
`MUL` and `DIV`:

    > (PLUS 5 5)
    10
    > (MUL 11 10)
    110
    > (PLUS 5 (MUL 10 10))
    105

Lists are constructed using the `CONS` primitive. To extract the first element
of a list, apply `CAR` to it. To extract the remainder (tail) of the list,
apply `CDR`. The emtpy list is known as `'()` or `NIL`.

    > (CONS 1 NIL)
    (1)
    > (CAR (CONS 1 (CONS 2 NIL)))
    1
    > (CDR (CONS 1 (CONS 2 NIL)))
    (2)

## Quotation
Any datum can be "quoted", meaning it is taken literally and returned
unevaluated:

    > (QUOTE 1)
    1
    > (QUOTE (1 2 3))
    (1 2 3)

The shorthand `'` can be used for the same effect:

    > '(1 2 3)
    (1 2 3)

## Conditionals
The `IF` form provides conditional tests. Everything that is not `NIL`
is true:

    > (IF 'FOO 'YES 'NO)
    YES
    > (IF CONS 'YES 'NO)
    YES
    > (IF '() 'YES 'NO)
    NO

## Lambdas
The `LAMBDA` form can be used to construct an anonymous function.
Turtles is lexically scoped, meaning that names bind to their lexically
"closest" definition:

    > (LAMBDA () 'FOO)
    #<LAMBDA>
    > ((LAMBDA (X) (PLUS X X)) 10)
    20

We have full support for closures, meaning that variables in a surrounding
scope are "captured" by the `LAMBDA` form. For example, in this case `X`
is captured by the inner lambda.

    > (((LAMBDA (X) (LAMBDA (Y) (PLUS X Y))) 5) 10)
    15

## Global definitions
Global names can be bound to values using the `DEFINE` form:

    > (DEFINE X 42)
    > X
    42

Global functions are easily defined by combining `DEFINE` with `LAMBDA`:

    > (DEFINE F (LAMBDA (X Y) (PLUS X Y)))
    > (F 2 3)
    5

## Basic recursion
We can easily define a recursive function to count the elements of a list:

    > (DEFINE LENGTH (LAMBDA (L)
                       (IF L
                         (PLUS 1 (LENGTH (CDR L)))
                         0)))

## Higher order programming
The forms we have explored so far are sufficient to define some common
higher order programming tools, such as `MAP`:

    > (DEFINE MAP (LAMBDA (F L)
                    (IF L
                      (CONS (F (CAR L)) (MAP F (CDR L)))
                      '())))

We can use this in conjunction with `LAMBDA` to square the elements of a list:

    > (MAP (LAMBDA (X) (MUL X X)) '(1 2 3)
    (1 4 9)

# Dependencies

* [apple2e-audio-transport](https://github.com/hausdorff/apple2e-audio-transport), a small library written specifically for this project. Sends the program over the audio jack to the Apple IIe.
* [sox](http://sox.sourceforge.net/), which we use to emit sound to the Apple IIe.
* [cpp](http://gcc.gnu.org/onlinedocs/cpp/), the C preprocessor; we write macros to make programming in the 6502 instruction set slightly easier.
* [xa](http://www.floodgap.com/retrotech/xa/)(aka xa65), assembler for the 6502 instruction set.

# LICENSE

Distributed under MIT, which basically means that if you should use this code for anything, you just have to keep a note saying we wrote the code. That said, God help you should you actually decide to use this code.


## MIT License

Copyright (C) 2013 Martin Törnwall (@mtornwall), Alex Clemmer (@hausdorff)

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
