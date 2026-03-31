# OptimizedGPIO

This fast General-Purpose Input/Output (GPIO) library is a single include file `OptimizedGPIO.h` that provides top speed optimized digital I/O for STM32, SAMD, AVR, ESP32 and ESP8266 boards. The right code for the board is selected automatically by `#ifdef` directives, so you don't need to do anything special. The same [API](#api) is used for each board, so no changes are needed if you change the board type.

This library was originally developed as part of a cross-platform Stepper Motor library, which will be released -soon- eventually.

The voluminous README text is aimed at fledgling Arduino developers. The rest of you probably know this stuff already.

_Joke of the Week: "His software had more bugs in it than the Amazon Rainforest". (Not referring to me, of course.)_

<!-- ================================================================================ -->

## Contents

- [Why do we need faster I/O?](#why-do-we-need-it)
- [Using the Library](#using-the-library)
- [OptimizedGPIO API](#api)
- [Example Sketch](#example-sketch)
- [Timing Comparisons](#timing-comparisons)
- [Disabling Interrupts](#disabling-interrupts)
- [Using OptimizedGPIO to bit-bang a serial shift register](#bit-banging)
<br/>

<!-- ================================================================================ -->

<a name="why-do-we-need-it"></a>

## Why do we need faster I/O?

The traditional `digitalRead()` and `digitalWrite()` functions are quite slow when compared to stripped-down code that only accesses the microcontroller's GPIO registers. The fast OptimizedGPIO versions in this library are typically 10 times faster, see [Timing Comparisons](#timing-comparisons).

The reason for this is that a lot of work is done for each access which could be done just once in a `begin()` method. This saves a lot of time when the program is running. Fast IO is particularly important in an interrupt handler (ISR), which must be as short as possible. It's also significant if you are doing a lot of "bit-banging" as in the [Using OptimizedGPIO to bit-bang a serial shift register](#bit-banging) example.

Below is the Arduino AVR code for `digitalWrite()`. The lines marked with `*` could be called in `begin()`. The lines marked with `**` are not needed if you are sure the output is not a PWM output. So most of the code can be transferred to `begin()`, leaving only the code that directly accesses the MCU's GPIO output register, marked with `\\>>> ... \\<<<`.

The standard libraries don't do this because calling `begin()` for each I/O you want to use makes it more complicated, and removing runtime error checks is dangerous. For this library you must create an object for each pin, and call its `begin()` method. See [Using the Library](#using-the-library) for details.

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

### `set()` and `reset()` methods

To make it even faster, instead of using a single `write()` method, it has separate `set()` and `reset()` methods to replace `digitalWrite(pin, 1)` and `digitalWrite(pin, 0)`. This just means that it doesn't have to do a check for `1` or `0` every time (`if (val == LOW` in the code above), which saves a bit of time. If you need it, there is a `write(bool b)` method too, but this just calls set and reset after checking `b`, `void write(bool b) { b ? set() : reset(); }`. You can modify the example project to see what effect this has on the speed.


<!-- ================================================================================ -->


<a name="using-the-library"></a>

## Using the Library

1. Install the library using the Arduino Library Manager, or download the ZIP file from github and install it with "Sketch / Include Library > Add .ZIP Library...". https://github.com/mumanchu/OptimizedGPIO

As a first step you could open the `OptimizedGPIOExample1.ino` sketch from "File / Examples > Examples from Custom Libraries". 

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
> If you see the warning message `#warning Unknown microcontroller, needs OptimizedGPIO class` when you build the program, it means that your MCU is not supported by the library, and the default (slow) `digitalRead()` and `digitalWrite()` functions will be used. It could also mean that the `#define` symbol used to select the MCU has not been defined. The library uses a single symbol to select the architecture, and these symbols have not been standardised on the Arduino platform. _If you get this warning, please email us on `info@muman.ch` with the details of your board, and we'll fix it for the next release._


<!-- ================================================================================ -->


<a name="api"></a>

## OptimizedGPIO API (Application Programming Interface)

The API is the same for every MCU type. This means you can use the same code in every project and for multi-platform projects without changing the code.

```cpp
bool begin(int pin, int mode);
```
Initializes the optimized pin object. Returns `false` if the pin number is invalid. 

```cpp
bool read();
```
Use this instead of `digitalRead()`.

```cpp
void write(bool b);
```
Writes a `1` or a `0` to the output pin. It's (slightly) faster to use `set()` and `reset()`.

```cpp
void set();
```
Sets the output to 1 (high). Use this instead of `digitalWrite(pin, 1)`.

```cpp
void reset();
```
Sets the output to 0 (low). Use this instead of `digitalWrite(pin, 1)`.

```cpp
void toggle();
```
This just sets the output high then low again. _(It's totally useless of your output is active low :-)_ This is a separate method because it may have to disable interrupts during the read-modify-write sequence to prevent an interrupt handler changing the register in between. This must disable interrupts, but only re-enable them again afterwards if they were enabled before. The code to do this is sometimes present but is commented out because it slows it down. You do not need to disable interrupts if there is no interrupt that could change the GPIO register. See [Disabling Interrupts](#disabling-interrupts).

<!-- ================================================================================ -->

<a name="example-sketch"></a>

## Example Sketch

The example sketch `OptimizedGPIOExample1.ino` outputs the time in microseconds for 100000 iterations of `digitalWrite()` and `digitalRead()`, along with the `OptimizedGPIO` versions, `set()` and `read()`.

You can see the timing results below. Try it with your own board and see what you get.

<details>
<summary>Click to see the Example Sketch code</summary>

###

```cpp
// Optimized GPIO Library Example
// muman.ch, 2026.03.31
// https://github.com/mumanchu/OptimizedGPIO

#include "OptimizedGPIO.h"

// The pins to be used for testing
#define TEST_OUTPUT_PIN LED_BUILTIN
#define TEST_INPUT_PIN 2
OptimizedGPIO testOutputPin;
OptimizedGPIO testInputPin;

// The number of times around the timing loop
#define LOOP_COUNT 100000

void setup() 
{
	Serial.begin(115200);

	// delay to give you time to open the serial monitor
	delay(3000);

	Serial.println("\n\rStarted...\n\r");
	Serial.flush();

	// initialize the test pins for optimized IO
	testOutputPin.begin(TEST_OUTPUT_PIN, OUTPUT);
	testInputPin.begin(TEST_INPUT_PIN, INPUT);
}

void loop() 
{
	unsigned long t;
	unsigned long elapsedTime;
	char buf[128];

	// time the timing loop so we can subtract it
	t = micros();
	for (volatile long i = 0; i < LOOP_COUNT; ++i) {
		;
	}
	unsigned long loopTime = micros() - t;

	sprintf(buf, "loopTime       = %lu microseconds", loopTime);
	Serial.println(buf);

	// time digitalWrite(TEST_PIN)
	t = micros();
	for (volatile long i = 0; i < LOOP_COUNT; ++i) {
		digitalWrite(TEST_OUTPUT_PIN, 1);
	}
	elapsedTime = micros() - t - loopTime;
	sprintf(buf, "digitalWrite() = %lu microseconds", elapsedTime);
	Serial.println(buf);

	// time testPin.set()
	t = micros();
	for (volatile long i = 0; i < LOOP_COUNT; ++i) {
		testOutputPin.set();
	}
	elapsedTime = micros() - t - loopTime;
	sprintf(buf, "set()          = %lu microseconds", elapsedTime);
	Serial.println(buf);

	// time digitalRead(TEST_PIN)
	t = micros();
	for (volatile long i = 0; i < LOOP_COUNT; ++i) {
		digitalRead(TEST_INPUT_PIN);
	}
	elapsedTime = micros() - t - loopTime;
	sprintf(buf, "digitalRead()  = %lu microseconds", elapsedTime);
	Serial.println(buf);

	// time testPin.read()
	t = micros();
	for (volatile long i = 0; i < LOOP_COUNT; ++i) {
		testInputPin.read();
	}
	elapsedTime = micros() - t - loopTime;
	sprintf(buf, "read()         = %lu microseconds", elapsedTime);
	Serial.println(buf);

	Serial.println();

	delay(1000);
}
```
</details>

<!-- ================================================================================ -->

<a name="timing-comparisons"></a>

## Timing Comparisons

The speed of each method can be computed using a loop which runs the method (say) 100000 times. The problem is that the looping itself takes a lot of time, and you need to remove that from the measurement. The over-efficient code optimizer may also 'optimize out' your loop, that's why the loop variable is `volatile`. The [Example Sketch](#example-sketch) shows the solution.

These are the timings I got from runnning the Example Sketch on several different boards. The table compares the standard `digitalRead()` and `digitWrite()` calls with the `OptimizedGPIO.set()` and `OptimizedGPIO.read()` methods for each board. You can see it makes a huge difference. This effect is accumulative if you are doing something intensive like "bit-banging" multiple bits into a shift register.

Timing for 100'000 digital read/write operations, in milliseconds. The empty loop time is already subtracted from the test loop time, but it's shown for comparison.


|                       | At328P AVR 16MHz| At2560 AVR 16MHz  | SAMD21 48MHz | SAMD51 120MHz     | ESP32 240MHz   | ESP32S3 240MHz | ESP32S3 240MHz    |
| :---------            | ---------:      | ---------:        | ---------:   | ---------:        | -----------:   | ---------:     | ---------:        |
| Board                 | Arduino Uno R3  | Arduino Mega 2560 | Arduino Zero | Adafruit Metro M4 | ESP32-WROOM-32 | ESP32 Mini     | Seeed Studio XIAO |
| Empty loop time       | 295.51          | 295.55            | 22.96        | 8.35              | 8.37           | 8.88           | 7.12              |
| digitalWrite()        | 239.18          | 471.88            | 158.83       | 40.89             | 31.45          | 52.41          | 49.42             |
| digitalRead()         | 201.44          | 459.29            | 104.56       | 24.20             | 19.70          | 38.99          | 39.38             |
| OptimizedGPIO.set()   | 69.42           | 69.45             | 8.55         | 0.83              | 6.28           | 5.09           | 4.61              |
| OptimizedGPIO.read()  | 12.83           | 12.85             | 12.72        | 2.50              | 5.87           | 3.84           | 6.71              |


|                       | STM32F103RB 72MHz|STM32F446RE 180MHz| STM32H747 480MHz| ESP8266 80MHz |
| :---------            | ---------:       |---------:        | ----------:     | -----------:  |
| Board                 | Nucleo-64        |Nucleo-64         | Arduino GIGA R1 | LOLIN D1 Mini |
| Empty loop time       | 25.03            |7.78              | 1.05            | 16.25         |
| digitalWrite()        | 192.56           |33.40             | 12.45           | 157.5         |
| digitalRead()         | 155.01           |28.93             | 11.59           | 58.75         |
| OptimizedGPIO.set()   | 25.14            |3.92              | 8.18            | 13.75         |
| OptimizedGPIO.read()  | 23.58            |3.92              | 3.57            | 21.25         |


> [!NOTE]
> The Arduino GIGA is the fastest board at 480MHz. But it is a two-core MCU, so it has extra code to handle conflicts if both processors try to access the same GPIO register. This is what slows down the `set()` method. \
> The second fastest board is the Adafruit Metro M4 (SAMD51). At 120MHz its GPIOs are faster than the 240MHz ESP32's, especially for `set()` and `reset()`. This is because the SAMD's GPIOs are far more efficient, using the `OUTSET` and `OUTCLR` registers to set or clear a single bit, so there's no need for read-modify-write or disabling/enabling interrupts.

<!-- ================================================================================ -->

<a name="disabling-interrupts"></a>

### Disabling Interrupts

If the code does a read-modify-write, then it must disable interrupts while updating the output register, but _only_ if another interrupt can modify the same output register. 

The problem with read-modify-write is that an interrupt might occur _between_ the read and the write which changes the register that was just read, and the subsequent write will cause those changes to be overwritten and lost. _This is a common and very nasty intermittent bug._

In the AVR `digitalWrite()` code at the start of this article, `SREG` is the AVR's 'Status Register' which contains the 'Global Interrupt Enable' bit. It first saves the `SREG` register with `uint8_t oldSREG = SREG;`, clears the bit (disables interrupts) with `cli()` (same as `noInterrupts()`), then restores the global interrupt state with `SREG = oldSREG;` afterwards. This is perfect, because it leaves the interrupt state in _the same state_ it was in before it was disabled, either enabled or disabled. 

If you were to use `sti()` or `interrupts()` instead of `SREG = oldSREG;` then it would enable interrupts as a side-effect. This is _very bad_ if you want them to remain disabled, and this can be fatal in an interrupt handler which expects interrupts to be disabled until it returns.

Some MCUs, like the SAMD, have special registers where you can write just one bit to set or clear a digital output, e.g. OUTSET and OUTCLR. This is great because it does not need a dangerous (and slow) read-modify-write operation to set an output, and you don't need to disable interrupts.

> [!CAUTION]
> The `toggle()` methods in this library do read-modify-write, but they _do not_ disable/enable interrupts. Some of them have patched-out code to do this. But you don't need to disable/enable interrupts if you _know_ that the output is _never_ modified by an interrupt. Disabling/enabling interrupts slows it down, so there's no point in doing it if you don't need it.


<!-- ================================================================================ -->

<a name="bit-banging"></a>

## Using OptimizedGPIO to bit-bang a serial shift register

This is a nice application for OptmizedGPIO and it makes a _huge_ speed improvement.

The cheapest way to extend the number of outputs of your MCU is to add a 'serial shift register' chip. This is a 'serial in parallel out' chip which lets you clock in bits serially, then transfer them to an 8-bit output register. These old chips cost just a few cents. 

Serial shift registers are also _very fast_ when compared to an expensive SPI or I2C I/O Expander like the Microchip MCP23017. See the blog entry, [MCP23xxx 8/16-Bit I2C/SPI GPIO Expander](https://muman.ch/muman/index.htm?muman-mcp23017.htm) for details of these chips, and more code.

You need 3 outputs to write to a serial shift register, DATA, CLOCK and STROBE (although the chip's pins may have different names). CLOCK clocks in a DATA bit on its rising edge. After 8 bits have been clocked in, STROBE transfers the data to the 8 outputs without any glitches.

There are several common serial shift register chips you can use, the 74HC595, CD4094 and the 74LS674, and there are probably more.

These chips can also be chained together, to get 16, 24 or 32 bits, or even more if you need it. The more bits, the longer it takes to update the outputs.



https://www.ti.com/lit/ds/symlink/sn74hc595.pdf \
https://www.ti.com/lit/ds/symlink/cd4094b.pdf \
https://www.ti.com/lit/ds/symlink/sn74ls674.pdf


TODO chip details

TODO wiring diagram


### Source code

<details>
<summary>Click to see the code</summary>

###

```cpp
```
</details>


### Timing for bit-banging a shift register

I used Arduino Zero for this, because it has the built-in EDBG debugger.

TODO 


