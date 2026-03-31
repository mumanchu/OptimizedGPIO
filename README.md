# OptimizedGPIO

This fast General-Purpose Input/Output (GPIO) library is a single include file `OptimizedGPIO.h` that provides top speed optimized digital I/O for STM32, SAMD, AVR, ESP32 and ESP8266 boards. The right code for the board is selected automatically by `#ifdef` directives, so you don't need to do anything special. The same [API](#api) is used for each board, so no changes are need if you change board type.

This voluminous README text is aimed at fledgling Arduino developers. The rest of you probably know this stuff already.

_Joke of the week: "His software had more bugs in it than the Amazon Rainforest". (Not referring to me, of course.)_


<!-- ================================================================================ -->

<a name="why-do-we-need-it"></a>

## Why do we need faster I/O?

The traditional `digitalRead()` and `digitalWrite()` functions are quite slow when compared to stripped-down code that only accesses the microcontroller's GPIO registers. The fast OptimizedGPIO versions are typically 10 times faster.

The reason for this is that a lot of work is done for each access which could be done in a `begin()` method. This would save a lot of time when the program is running. Fast IO is particularly important in an interrupt handler (ISR), which must be as short as possible. It's also significant if you are doing a lot of "bit-banging" as in the [Using OptimizedGPIO to bit-bang a serial shift register](#bit-banging) example.

Below is the Arduino code for `digitalWrite()`. The lines marked with `*` could be called in `begin()`. The lines marked with `**` are not needed if you are sure the output is not a PWM output. So most of the code can be transferred to `begin()`, leaving only the code that directly accesses the MCU's GPIO output register, marked with `\\>>> ... \\<<<`.

The standard libraries don't do this because calling `begin()` for each I/O you want to use makes it more complicated and prone to errors. For this library you must create an object for each pin, and call its `begin()` method. See [Using the Library](#using-the-library) for details.

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


You can find the Arduino code for digital I/O in the Arduino "packages" subdirectory for the MCU, in a file called `wiring_digital.c`, usually in a subdirectory of `cores`. This file is where most of the code for this library originated. For example, you will find the AVR code here (the version number may be different):
```
C:\Users\<user-name>\AppData\Local\Arduino15\packages\arduino\hardware\avr\1.8.7\cores\arduino\wiring_digital.c
```

### `set()` and `reset()` methods

To make it even faster, instead of using a single `write()` method, it has separate `set()` and `reset()` methods to replace `digitalWrite(pin, 1)` and `digitalWrite(pin, 0)`. This just means that it doesn't have to do a check for `1` or `0` every time, which saves a bit of time. If you need it, there is a `write(bool b)` method too, but this just calls set and reset after checking `b`, `void write(bool b) { b ? set() : reset(); }`. You can modify the example project to see what effect this has on the speed - it's noticeable.


<!-- ================================================================================ -->


<a name="using-the-library"></a>

## Using the Library

1. Install the library using the Arduino Library Manager, or download the ZIP file from github and install it with "Sketch / Include Library > Add .ZIP Library...". 
https://github.com/mumanchu/OptimizedGPIO

As a first step you could open the `OptimizedGPIOExample1.ino` sketch from "File / Examples > Examples from Custom Libraries". Or create your own sketch and...

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
> If you see the warning message `#warning Unknown microcontroller, needs OptimizedGPIO class` when you build the program, it means that your MCU is not supported by the library, and the default (slow) `digitalRead()` and `digitalWrite()` functions will be used. It could also mean that the `#define` symbol used to select the MCU has not been #defined. The library uses a single symbol to select the architecture, and these symbols have not been standardised on the Arduino platform. _If you get this warning, please email us on `info@muman.ch` with the details of your board, and we'll fix it for the next release._


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
This just sets the output high then low again. _(It's totally useless of your output is active low :-)_ This is a separate method because it may have to disable interrupts during the read-modify-write sequence to prevent an interrupt handler changing the register in between. This must disable interrupts, but only re-enable them again afterwards if they were enabled before. The code to do this is present but is commented out because it slows it down. You do not need to disable interrupts if there is no interrupt that could change the GPIO register.

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
> The Arduino GIGA is the fastest board at 480MHz. But it is a two-core MCU, so it has extra code to handle conflicts if both processors try to access the same GPIO register. This seems to slow it down quite a lot.\
> The second fastest board is the Adafruit Metro M4 (SAMD51). At 120MHz it is faster than the 240MHz ESP32's, especially for `set()` and `reset()`. This is because the SAMD's GPIOs are far more efficient, using the `OUTSET` and `OUTCLR` registers to set or clear a single bit.

<!-- ================================================================================ -->

<a name="disabling-interrupts"></a>

### Disabling Interrupts

The above code also does a read-modify-write, which is why it must disable interrupts while updating the output register. SREG is the AVR's 'Status Register' which contains the 'Global Interrupt Enable' bit. It first saves the SREG register with `uint8_t oldSREG = SREG;`, clears the bit (disables interrupts) with `cli()` (same as `noInterrupts()`), then restores the global interrupt state with `SREG = oldSREG;` afterwards. This is good, because it leaves the interrupt state in the same state it was in before, either enabled or disabled. 

If you were to use `sti()` or `interrupts()` instead of `SREG = oldSREG;` then it would enable interrupts as a side-effect. This is _very bad_ if you want them to remain disabled, and can be fatal in an interrupt handler which expects interrupts to be disabled until it returns.

The problem with read-modify-write is that an interrupt might occur _between_ the read and the write which changes the register that was just read, and the subsequent write will cause those changes to be overwritten and lost (a very nasty intermittent bug).

This is also a problem with the `toggle()` methods. The `toggle()` methods in this library have patched-out code to prevent this happening. You don't need this code if you _know_ that the output is _never_ modified by an interrupt, which is why it is patched out. Disabling/enabling interrupts also slows it down, so there's no point in doing it if you don't need it.

Some MCUs, like the SAMD, have special registers where you can write just one bit to set or clear a digital output, e.g. OUTSET and OUTCLR. This is great because it does not need a dangerous (and slower) read-modify-write operation to set an output, and you don't need to disable interrupts.

<!-- ================================================================================ -->

<a name="bit-banging"></a>

## Using OptimizedGPIO to bit-bang a serial shift register

This is a nice application for OptmizedGPIO and it makes a _huge_ speed improvement.

The cheapest way to extend the number of outputs of your MCU is to add a 'serial shift register' chip. This is a 'serial in parallel out' chip which lets you clock in bits serially, then transfer them to an 8-bit output register. These old chips cost just a few cents. 

Serial shift registers are also _very fast_ when compared to an SPI or I2C I/O Expander like the Microchip MCP23017. See the blog entry, [MCP23xxx 8/16-Bit I2C/SPI GPIO Expander](https://muman.ch/muman/index.htm?muman-mcp23017.htm) for details.

You need 3 outputs to write to a serial shift register, DATA, CLOCK and STROBE (although the chip's pins may have different names). CLOCK clocks in a DATA bit on its rising edge. After 8 bits have been clocked in, STROBE transfers the data to the outputs, without any glitches.

There are several serial shift register chips you canuse, the 74HC595, CD4095 and the 74LS674, and there are probably more.



https://www.ti.com/lit/ds/symlink/sn74ls674.pdf

These chips can also be chained together, to get 16, 24 or 32 bits, or even more if you need it. The more bits, the slower the update speed.

TODO chip details

TODO wiring diagram

TODO source code


### Timing for bit-banging a shift register

TODO 



### Source code

<details>
<summary>Click to see the code</summary>

###

```cpp
```
</details>


<!--
gpio_object.h


/* mbed Microcontroller Library
 *******************************************************************************
 * Copyright (c) 2016, STMicroelectronics
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of STMicroelectronics nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *******************************************************************************
 */
#ifndef MBED_GPIO_OBJECT_H
#define MBED_GPIO_OBJECT_H

#include "mbed_assert.h"
#include "cmsis.h"
#include "PortNames.h"
#include "PeripheralNames.h"
#include "PinNames.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Note: reg_clr might actually be same as reg_set.
 * Depends on family whether BRR is available on top of BSRR
 * if BRR does not exist, family shall define GPIO_IP_WITHOUT_BRR
 */
typedef struct {
    uint32_t mask;
    __IO uint32_t *reg_in;
    __IO uint32_t *reg_set;
    __IO uint32_t *reg_clr;
    PinName  pin;
    GPIO_TypeDef *gpio;
    uint32_t ll_pin;
} gpio_t;

static inline void gpio_write(gpio_t *obj, int value)
{
#if defined(DUAL_CORE) && (TARGET_STM32H7)
    while (LL_HSEM_1StepLock(HSEM, CFG_HW_GPIO_SEMID)) {
    }
#endif /* DUAL_CORE */

    if (value) {
        *obj->reg_set = obj->mask;
    } else {
#ifdef GPIO_IP_WITHOUT_BRR
        *obj->reg_clr = obj->mask << 16;
#else
        *obj->reg_clr = obj->mask;
#endif
    }

#if defined(DUAL_CORE) && (TARGET_STM32H7)
    LL_HSEM_ReleaseLock(HSEM, CFG_HW_GPIO_SEMID, HSEM_CR_COREID_CURRENT);
#endif /* DUAL_CORE */
}

static inline int gpio_read(gpio_t *obj)
{
    return ((*obj->reg_in & obj->mask) ? 1 : 0);
}

static inline int gpio_is_connected(const gpio_t *obj)
{
    return obj->pin != (PinName)NC;
}


#ifdef __cplusplus
}
#endif

#endif


-->




<!--

Nucleo-64 STM32F103RB 72MHz/128KB/20KB
loopTime       = 25037 microseconds
digitalWrite() = 192558 microseconds
set()          = 25144 microseconds
digitalRead()  = 155010 microseconds
read()         = 23579 microseconds



Nucleo-64 STM32F446RE 180MHz/512KB/128KB/FPU
loopTime       = 7782 microseconds
digitalWrite() = 33400 microseconds
set()          = 3924 microseconds
digitalRead()  = 28932 microseconds
read()         = 3925 microseconds


XIAO ESP32S3  240MHz/8MB/8MB/FPU
loopTime       = 7123 microseconds
digitalWrite() = 49425 microseconds
set()          = 4609 microseconds
digitalRead()  = 39384 microseconds
read()         = 6713 microseconds



TODO ESP8266


Default optimization, Serial debug


Arduino Uno R3, ATmega328P 16MHz
loopTime       = 295508 microseconds
digitalWrite() = 239176 microseconds
set()          = 69416 microseconds
digitalRead()  = 201440 microseconds
read()         = 12832 microseconds


Arduino Mega 2560, ATmega2560 AVR 16MHz
loopTime       = 295548 microseconds
digitalWrite() = 471884 microseconds
set()          = 69448 microseconds
digitalRead()  = 459292 microseconds
read()         = 12852 microseconds


Arduino Zero, SAMD21 48MHz
loopTime       = 22963 microseconds
digitalWrite() = 158832 microseconds
set()          = 8546 microseconds
digitalRead()  = 104564 microseconds
read()         = 12722 microseconds


Adafruit Metro M4 Express, SAMD51 120MHz
loopTime       = 8346 microseconds
digitalWrite() = 40894 microseconds
set()          = 834 microseconds
digitalRead()  = 24203 microseconds
read()         = 2504 microseconds



ESP32 Wemos D32 R1 ESPDUINO-32 HW-729   240MHz/4MB/520K
loopTime       = 8367 microseconds
digitalWrite() = 31452 microseconds
set()          = 6284 microseconds
digitalRead()  = 19702 microseconds
read()         = 5869 microseconds

WEMOS/LOLIN ESP32S2 MINI  240MHz/4MB/2MB
loopTime       = 8880 microseconds
digitalWrite() = 52409 microseconds
set()          = 5095 microseconds
digitalRead()  = 38993 microseconds
read()         = 3840 microseconds



Arduino Giga R1, STM32H747 240MHz/480MHz 2MB/1MB
loopTime       = 1051 microseconds
digitalWrite() = 12450 microseconds
set()          = 8180 microseconds
digitalRead()  = 11587 microseconds
read()         = 3571 microseconds

LOLIN WEMOS D1 mini
loopTime       = 16254 microseconds
digitalWrite() = 157500 microseconds
set()          = 13751 microseconds
digitalRead()  = 58750 microseconds
read()         = 21251 microseconds



// C:\Users\...\AppData\Local\Arduino15\packages\STMicroelectronics\hardware\stm32\2.12.0\cores\arduino\wiring_digital.c
// C:\Users\...\AppData\Local\Arduino15\packages\arduino\hardware\samd\1.8.14\cores\arduino\wiring_digital.c
// C:\Users\...\AppData\Local\Arduino15\packages\esp8266\hardware\esp8266\3.1.2\cores\esp8266\core_esp8266_wiring_digital.cpp
// C:\Users\...\AppData\Local\Arduino15\packages\arduino\hardware\avr\1.8.7\cores\arduino\wiring_digital.c
//C:\Users\matth\AppData\Local\Arduino15\packages\arduino\hardware\mbed_giga\4.5.0\cores\arduino\mbed\drivers\include\drivers\DigitalInOut.h
//C:\Users\matth\AppData\Local\Arduino15\packages\arduino\hardware\mbed_giga\4.5.0\cores\arduino\wiring_digital.cpp





-->

