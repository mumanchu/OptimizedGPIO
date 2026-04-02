# OptimizedGPIO

This fast General-Purpose Input/Output (GPIO) library uses a single include file `OptimizedGPIO.h` to provide top-speed optimized digital I/O for STM32, SAMD, AVR, ESP32 and ESP8266 boards. The right code for the board is selected automatically by `#ifdef` directives, so you don't need to do anything special. The same [API](#api) is used for each board, so no changes are needed to your code if you change the board type.

This library was originally developed as part of a cross-platform Stepper Motor library, which will be released ~~soon~~ eventually.

The voluminous README text is aimed at fledgling Arduino developers. The rest of you probably know this stuff already.

_Joke of the Week: "His software had more bugs in it than the Amazon Rainforest". (Not referring to me, of course.)_


<!-- ================================================================================ -->

## Contents

- [Why do we need faster I/O?](#why-do-we-need-it)
- [Using the Library](#using-the-library)
- [The OptimizedGPIO API](#api)
- [Example Sketch](#example-sketch)
- [Timing Comparisons](#timing-comparisons)
- [Existing digitalReadFast() and digitalWriteFast() functions](#digital-read-fast)
- [Disabling Interrupts?](#disabling-interrupts)
- [Using OptimizedGPIO to bit-bang a serial shift register (Output Expander)](#bit-banging)
- [Reference Links](#reference-links)
- [Revision History](#revision-history)

<!-- ================================================================================ -->

<a name="why-do-we-need-it"></a>

## Why do we need faster I/O?

The traditional `digitalRead()` and `digitalWrite()` functions are quite slow when compared to stripped-down code that only accesses the microcontroller's GPIO registers. The fast OptimizedGPIO versions in this library are typically 10 times faster, see [Timing Comparisons](#timing-comparisons).

The reason for this is that a lot of work and validation is done for every access which could be done _just once_ in a `begin()` method. This saves a lot of time when the program is running. The drawback is that runtime validation is not done, so if `begin()` returns `false`, then acessing the I/O will have "unexpected" results. 

Fast IO is particularly important in an interrupt handler (ISR), which must be as short as possible. It's also significant if you are doing a lot of "bit-banging" as in the [Using OptimizedGPIO to bit-bang a serial shift register](#bit-banging) example.

Below is the Arduino AVR code for `digitalWrite()`. The lines marked with `*` could be called in `begin()`. The lines marked with `**` are not needed if you are sure the output is not a PWM output. So most of the code can be transferred to `begin()`, leaving only the code that directly accesses the MCU's GPIO output register, marked with `\\>>> ... \\<<<`.

For this library you must create an object for each pin and call its `begin()` method. See [Using the Library](#using-the-library) for details.

```cpp
void digitalWrite(uint8_t pin, uint8_t val)
{
	uint8_t timer = digitalPinToTimer(pin); //**
	uint8_t bit = digitalPinToBitMask(pin); //*
	uint8_t port = digitalPinToPort(pin);   //*
	volatile uint8_t *out;

	if (port == NOT_A_PIN) return;  //*

	// If the pin that support PWM output, we need to turn it off
	// before doing a digital write.
	if (timer != NOT_ON_TIMER) turnOffPWM(timer); //**

	out = portOutputRegister(port); //*

	//>>>
	uint8_t oldSREG = SREG;
	cli();
	
	if (val == LOW) {
		*out &= ~bit;
	} else {
		*out |= bit;
	}

	SREG = oldSREG;
	//<<<
}
```
See the note about [Disabling Interrupts](#disabling-interrupts) to find out about `SREG`.

You can find the Arduino code for digital I/O in the Arduino "packages" subdirectory for the MCU, in a file called `wiring_digital.c`, usually in a subdirectory of `cores`. This file is where most of the code for this library originated. For example, you will find the AVR code here (the version number may be different):
```
C:\Users\<user-name>\AppData\Local\Arduino15\packages\arduino\hardware\avr\1.8.7\cores\arduino\wiring_digital.c
```

**`set()` and `reset()` methods**

To make it even faster, instead of using a single `write()` method, it has separate `set()` and `reset()` methods to replace `digitalWrite(pin, 1)` and `digitalWrite(pin, 0)`. This just means that it doesn't have to do a check for `1` or `0` every time (`if (val == LOW` in the code above), which saves a bit of time. If you need it, there is a `write(bool b)` method too, but this just calls set and reset after checking `b`, `void write(bool b) { b ? set() : reset(); }`. You can modify the example project to see what effect this has on the speed.


<!-- ================================================================================ -->


<a name="using-the-library"></a>

## Using the Library

1. Install the library using the Arduino Library Manager, or download the ZIP file from github and install it with "Sketch / Include Library > Add .ZIP Library...". https://github.com/mumanchu/OptimizedGPIO

As a first step you could open the `OptimizedGPIO.ino` sketch from "File / Examples > Examples from Custom Libraries". 

Or create your own sketch and...

2. Include the file `OptimizedGPIO.h`. The right code for your MCU is automatically selected by the `$ifdef` statements in the include file. 
```cpp
#include "OptimizedGPIO.h"
```

3. Declare an `OptimizedGPIO` object for each pin you want to use.
```cpp
OptimizedGPIO stepPin;
OptimizedGPIO inputPin;
```

4. In `setup()`, call `begin(pin, mode)` for each pin, where the `mode` is `INPUT` or `OUTPUT`, or maybe `INPUT_PULLUP`, `INPUT_PULLDOWN`, `OUTPUT_OPEN_DRAIN` etc. if your MCU supports them.
```cpp
void setup() {
	...
	stepPin.begin(2, OUTPUT);
	inputPin.begin(3, INPUT);
}
```

5. Use the OptimizedGPIO object's methods to access the pin.
```cpp
	stepPin.set();		// pin := 1
	stepPin.reset();	// pin := 0
	bool b = inputPin.read();
```

> [!WARNING]
> If you see the warning message `#warning Unknown microcontroller, needs OptimizedGPIO class` when you build the program, it means that your MCU is not supported by the library, and the default (slow) `digitalRead()` and `digitalWrite()` functions will be used. It could also mean that the `#define` symbol used to select the MCU has not been defined. The OptimizedGPIO library uses a single symbol to select the architecture, and these symbols have not been standardised on the Arduino platform. _If you get this warning, please email us on `info@muman.ch` with the details of your board, and we'll fix it for the next release._


<!-- ================================================================================ -->


<a name="api"></a>

## The OptimizedGPIO API (Application Programming Interface)

The API is the same for all architectures. This means you can use the same code in every project and for multi-platform projects without changing the code.

**`bool begin(int pin, int mode);`** \
Initializes the optimized pin object. Returns `false` if the pin number is invalid.

**`bool read();`** \
Use this instead of `digitalRead()`.

**`void set();`** \
Sets the output to 1 (high). Use this instead of `digitalWrite(pin, 1)`.

**`void reset();`** \
Sets the output to 0 (low). Use this instead of `digitalWrite(pin, 1)`.

**`void write(bool b);`** \
Writes a `1` or a `0` to the output pin. It's (slightly) faster to use `set()` and `reset()`.

**`void toggle();`** \
This inverts the state of an output, using a read-modify-write sequence. This has to disable interrupts during the read-modify-write sequence to prevent an interrupt handler changing the register in between. It _should_ only re-enable them again afterwards if they were enabled before, but that's not always possible. See [Disabling Interrupts](#disabling-interrupts).

**`uint getPin();`** \
An inline 'getter' method (a concept from C#) that returns the `pin` number. Anorak: If `pin` was `public`, then you'd be able to write to it, which would be very naughty. If a private variable has a 'getter' method, then it can only be read, not written.


> [!NOTE]
> `pinMode()` should not be used after calling `OptimizedGPIO.begin()`, but you _can_ call the old `digitalRead()` and `digitalWrite()` on pins that have been initialised by `OptimizedGPIO.begin()`.
> This means that with `OptimizedGPIO`, you cannot change the mode of a pin at runtime. But it is rare that anyone would want to do that.


<!-- ================================================================================ -->

<a name="example-sketch"></a>

## Example Sketch

The example sketch `OptimizedGPIO.ino` outputs the time in microseconds for 100000 iterations of `digitalRead()` and `digitalWrite()`, along with the times for the `OptimizedGPIO` versions, `read()` and `set()`.

You can see the timing comparisons below. Try it with your own board and see what you get.

https://github.com/mumanchu/OptimizedGPIO/blob/main/examples/OptimizedGPIO/OptimizedGPIO.ino


<a name="timing-comparisons"></a>

### Timing Comparisons
In the example sketch, the speed of each method is computed using a loop which runs the method (say) 100000 times. The looping itself takes a lot of time, so you have to remove that overhead from the measurement. The over-efficient code optimizer may also 'optimize out' your loop, that's why the loop variable is `volatile`. The Example Sketch shows the solution.

These are the timings I got from runnning the Example Sketch on several different boards. The table compares the standard `digitalRead()` and `digitWrite()` calls with the `OptimizedGPIO.read()` and `OptimizedGPIO.set()` methods for each board. You can see it makes a huge difference. This effect is accumulative if you are doing something intensive like "bit-banging" multiple bits into (or outof) a shift register.

Timing for 100'000 digital read/write operations, in milliseconds. The empty loop time is already subtracted from the test loop time, but it's shown for comparison.

The program was built with "Default Optimization". Compiler optimizations will affect the results.


|                       | At328P AVR 16MHz| At2560 AVR 16MHz  | SAMD21 48MHz | SAMD51 120MHz     | ESP32 240MHz   | ESP32S3 240MHz | ESP32S3 240MHz    |
| :---------            | ---------:      | ---------:        | ---------:   | ---------:        | -----------:   | ---------:     | ---------:        |
| Board                 | Arduino Uno R3  | Arduino Mega 2560 | Arduino Zero | Adafruit Metro M4 | ESP32-WROOM-32 | ESP32 Mini     | Seeed Studio XIAO |
| Empty loop time       | 295.51          | 295.55            | 22.96        | 10.02             | 8.37           | 8.88           | 66.98              |
| digitalRead()         | 201.45          | 459.29            | 106.65       | 24.20             | 19.70          | 38.99          | 39.81             |
| digitalWrite()        | 239.18          | 471.88            | 156.77       | 40.90             | 31.45          | 52.41          | 51.10             |
| OptimizedGPIO.read()  | 12.83           | 12.85             | 12.71        | 5.84              | 5.87           | 3.84           | 7.13              |
| OptimizedGPIO.set()   | 88.28           | 69.45             | 8.55         | 4.17              | 6.28           | 5.09           | 4.12              |


|                       | STM32F103RB 72MHz|STM32F446RE 180MHz| STM32H747 480MHz| ESP8266 80MHz |
| :---------            | ---------:       | ---------:       | ----------:     | -----------:  |
| Board                 | Nucleo-64        | Nucleo-64        | Arduino GIGA R1 | LOLIN D1 Mini |
| Empty loop time       | 25.04            | 7.78             | 1.05            | 16.25         |
| digitalRead()         | 155.01           | 28.93            | 11.68           | 58.75         |
| digitalWrite()        | 187.86           | 33.38            | 12.83           | 157.51        |
| OptimizedGPIO.read()  | 25.15            | 3.92             | 3.57            | 21.25         |
| OptimizedGPIO.set()   | 22.01            | 3.92             | 8.18            | 13.75         |


> [!NOTE]
> The Arduino GIGA is the fastest board at 480MHz. But it is a two-core MCU, so it has extra code to handle conflicts if both processors try to access the same GPIO output register. This is why the `set()` method is so slow. \
> The STM32F446 at 180MHz is the fastest for the GPIOs. \
> The second fastest board is the Adafruit Metro M4 (SAMD51). At 120MHz its GPIOs are about the same the 240MHz ESP32's, especially for `set()` and `reset()`. \
> The Arduino code for the GIGA board has sort-of implemented a `begin()` method internally. The first time a pin function is called, it checks for an `mbed::DigitalInOut* gpio` object for that pin, and if not present it will create and initialize it, and re-use if for the next call.


<!-- ================================================================================ -->

<a name="digital-read-fast"></a>

## Existing digitalReadFast() and digitalWriteFast() functions (if present)

You may find that you already have `digitalReadFast()` and `digitalWriteFast()` functions, but not all architectures have implemented them. 

There is also a `pinModeFast()` which may be useful if you want to quicky switch a pin between input and output modes. Note that `pinMode()` and `pinModeFast()`should _never_ be used after calling `OptimizedGPIO.begin()` for the same pin, because the OptimizedGPIO methods may not work after that.

These 'fast' functions cut out some of the overhead, but not all. They are about 2x faster than the standard functions, but nowhere near as fast as `OptimizedGPIO`. Here are the timing comparisons for the STM32F103RB Nucleo-64 (72MHz).

|                       | STM32F103RB 72MHz |
| :---------            | ---------:        |
| digitalRead()         | 155.01            |
| digitalWrite()        | 187.86            |
| digitalReadFast()     | >>> 81.47         |
| digitalWriteFast()    | >>> 73.65         |
| OptimizedGPIO.read()  | 25.15             |
| OptimizedGPIO.set()   | 22.01             |

As you can see, the `OptimizedGPIO` methods are much faster than `digitalRead/WriteFast()`.

<!-- ================================================================================ -->

<a name="disabling-interrupts"></a>

## Disabling Interrupts?

If the code does a read-modify-write, then it should disable interrupts while updating the output register, but you only need to do that if another interrupt can modify _the same_ output register. Sometimes it's not so easy to find out. 

The problem with read-modify-write is that the interrupt might occur _between_ the read and the write. This changes the register that was just read, and the subsequent write will cause those changes to be overwritten and lost. _This is a common and very nasty intermittent bug._

In the AVR `digitalWrite()` code at the start of this article, `SREG` is the AVR's 'Status Register' which contains the 'Global Interrupt Enable' bit. It first saves the `SREG` register with `uint8_t oldSREG = SREG;`, clears the bit (disables interrupts) with `cli()` (same as `noInterrupts()`), then restores the global interrupt state with `SREG = oldSREG;` afterwards. This is perfect, because it leaves the interrupt state in _the same state_ it was in before it was disabled, either enabled or disabled.

If you were to use `sti()` or `interrupts()` instead of `SREG = oldSREG;` then it would enable interrupts as a side-effect. This is _very bad_ if you want them to remain disabled, and this can be fatal in an interrupt handler which expects interrupts to be disabled until it returns.

Many MCUs, like the SAMD and the ESP32, have special registers where you can write just one bit to set or clear a digital output, e.g. the SAMD's `OUTSET` and `OUTCLR`, or the ESP32's `GPIO_OUT_W1TS_REG` (write-1-to-set) and `GPIO_OUT_W1TC_REG` (write-1-to-clear). This is great because it does not need a dangerous (and slow) read-modify-write operation to set an output, and you don't need to disable interrupts.

> [!NOTE]
> The `toggle()` methods illustrate the code to save, disable and restore the interrupt state for the particular MCU. But some of them just use `noInterrupts()` and `interrupts()` so these should _NOT_ be called from an interrupt handler (ISR).

<!-- ESP32 does not have a single simple "interrupts on/off" flag like older AVR Arduinos. -->

<!-- ================================================================================ -->

<a name="bit-banging"></a>

## Using OptimizedGPIO to bit-bang a serial shift register (Output Expander)

This is a nice application for OptmizedGPIO and it makes a _huge_ speed improvement.

The cheapest (and fastest) way to extend the number of outputs of your MCU is to add a 'serial shift register' chip. This is a 'serial-in-parallel-out' chip which lets you clock bits serially into the chip, then transfer them to an 8-bit parallel output register. These old chips cost just a few cents.

Serial shift registers are also _extremely fast_ when compared to expensive SPI or I2C I/O Expanders like the Microchip MCP23017. See the blog entry, [MCP23xxx 8/16-Bit I2C/SPI GPIO Expander](https://muman.ch/muman/index.htm?muman-mcp23017.htm) to find out more about those. 

You need 3 outputs to write to a serial shift register chip: DATA, CLOCK and STROBE (although the chip's pins may have different names). CLOCK clocks in a DATA bit on its rising edge. After 8 bits have been clocked in, STROBE transfers the data to the 8 outputs without any glitches. 

Most chips also have a CLEAR pin which resets the outputs, but you don't have to use it - just write `0x00`. Some have tri-state outputs (they are 'floating' unless switched to GND), so there's an /ENABLE pin too - tie that GND unless you need tri-state outputs.

There are several common serial shift register chips you can use, the 74HC594, 74HC595, 74HC164 and the CMOS CD4094B, and there are probably more. The HC versions run on 3.3V _or_ 5V, the LS versions are _only_ for 5V MCUs.

Most of these 8-bit chips can be chained together to get 16, 24 or 32 bits, or even more. But the more bits, the longer it takes to update the outputs. The example code only works for an 8-bit shift register, but it's easy to modify it for 16/24/32 or more bits.

To expand the number _inputs_, you can do the reverse with a 'parallel-in-serial-out' chip like the 74HC165, 74HC166 and the CMOS CD4014B. But that's for your homework.

**Here's the timing for the shift-register Output Expander** \
Writing a byte using an I2C expander at 1MHz = 180uS \
Writing a byte using digitalWrite() = 31uS \
Writing a byte using OptimizedGPIO = 3uS !

The source code for the example is here:
https://github.com/mumanchu/OptimizedGPIO/blob/main/examples/OutputExpander8/OutputExpander8.ino


<!-- ================================================================================ -->

<a name="reference-links"></a>

## Reference Links

**Matt's Blog** \
Miscellaneous Arduino/ESP32/STM32 code, sensor evaluations, electronic designs, futile research, antigravity machines, etc. \
https://muman.ch/muman/index.htm?muman-matts-blog.htm

**74HC595 8-Bit Serial Shift Register** \
https://www.ti.com/lit/ds/symlink/sn74hc595.pdf

**Visual Micro extension for Microsoft Visual Studio** \
I use this for all my Arduino-style projects \
_"Compile and upload any Arduino project to any board, using the same Arduino platform and libraries, with all the advantages of the advanced Microsoft Visual Studio IDE"_ \
https://www.visualmicro.com/

<!-- ================================================================================ -->

<a name="revision-history"></a>

## Revision History

| Date       | Version  | Description |
|:---------- |:---------|:----------- |
| 2026.xx.xx | 1.0.0	| The first version! |

