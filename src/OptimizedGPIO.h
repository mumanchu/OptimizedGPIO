#pragma once

/////////////////////////////////////////////////////////////////////////
// Optimized Digital I/O for STM32, SAMD, AVR, ESP32, ESP8266 
// Fast replacements for the rather slow digitalRead() and digitalWrite()
// Uses direct access to the MCU's GPIO registers
// muman.ch, 2026.04.03
// https://github/mumanchu/OptimizedGPIO
/*
 All methods are inline code except for begin().
 For speed, no error checking is done except by pinMode() in begin()
 (which may or may not be reported, and may go into InfiniteLoop)
 
 Do not write to inputs or PWM outputs, the behaviour is undefined!

 If outputs on a port can be modified by an interrupt, toggle() and
 other methods which do read-modify-write must disable interrupts,
 see commented out code below. But this makes them slower, so only
 disable interrupts if the output is modified by an interrupt handler.
*/

#include <Arduino.h>

#ifndef PNUM_NOT_DEFINED
#define PNUM_NOT_DEFINED (-1)
#endif
#ifndef uint
typedef unsigned int uint;
typedef unsigned long ulong;
#endif


/////////////////////////////////////////////////////////////////////
// STM32

#if defined(STM32_CORE_VERSION)

class OptimizedGPIO
{
	uint pin = PNUM_NOT_DEFINED;
	GPIO_TypeDef* volatile port;
	uint mask;
public:
	bool begin(uint pin, uint mode);
	bool read() { return (port->IDR & mask) != 0; }
	uint getPin() { return pin; }
	// do not use these with INPUT pins!
	void write(bool b) { b ? set() : reset(); }
	void set() { port->BSRR = mask; }
	void reset() { port->BSRR = mask << 16; }
	void toggle();
};

bool OptimizedGPIO::begin(uint pin, uint mode)
{
	if (digitalPinToPinName(pin) == NC || mode == INPUT_ANALOG)
		return false;
	this->pin = pin;
	pinMode(pin, mode);
	port = digitalPinToPort(pin);
	mask = digitalPinToBitMask(pin);
	return port != NULL && mask != NC;
}

inline void OptimizedGPIO::toggle()
{
	bool interruptsEnabled = !(__get_PRIMASK() & 1);
	noInterrupts();
	port->ODR ^= pin;
	if (interruptsEnabled)
		interrupts();
}


/////////////////////////////////////////////////////////////////////
// SAMD

#elif defined(_SAMD21_) || defined(_SAMD51_)

class OptimizedGPIO
{
	uint pin = PNUM_NOT_DEFINED;
	uint mask;
	PortGroup* port;
public:
	bool begin(uint pin, uint mode);
	bool read() { return (port->IN.reg & mask) != 0; }
	uint getPin() { return pin; }
	// do not use these with INPUT pins!
	void write(bool b) { b ? set() : reset(); }
	void set() { port->OUTSET.reg = mask; }
	void reset() { port->OUTCLR.reg = mask; }
	void toggle();
};

bool OptimizedGPIO::begin(uint pin, uint mode)
{
	if (pin >= PINS_COUNT || g_APinDescription[pin].ulPinType == PIO_NOT_A_PIN)
		return false;
	this->pin = pin;
	pinMode(pin, mode);
	mask = 1 << g_APinDescription[pin].ulPin;
	port = PORT->Group + g_APinDescription[pin].ulPort;
	return true;
}

// WARNING: This enables interrupts, do not call from an ISR!
inline void OptimizedGPIO::toggle()
{
	noInterrupts();
	read() ? reset() : set();
	interrupts();
}


/////////////////////////////////////////////////////////////////////
// ESP32

#elif defined(ESP32)

class OptimizedGPIO
{
	uint pin = PNUM_NOT_DEFINED;
	uint mask;
	uint ts_reg;
	uint tc_reg;
	uint in_reg;
public:
	bool begin(uint pin, uint mode);
	bool read() { return REG_GET_BIT(in_reg, mask) != 0; }
	uint getPin() { return pin; }
	// do not use these with INPUT pins!
	void write(bool b) { b ? set() : reset(); }
	void set() { REG_WRITE(ts_reg, mask); }
	void reset() { REG_WRITE(tc_reg, mask); }
	void toggle();
};

bool OptimizedGPIO::begin(uint pin, uint mode)
{
	if (pin >= SOC_GPIO_PIN_COUNT)
		return false;
	this->pin = pin;
	pinMode(pin, mode);

	if (pin < 32) {
		mask = 1 << pin;
		ts_reg = GPIO_OUT_W1TS_REG;
		tc_reg = GPIO_OUT_W1TC_REG;
		in_reg = GPIO_IN_REG;
	}
	// some don't have more than 32 pins
	#ifdef GPIO_OUT1_W1TS_REG
	else {
		mask = 1 << (pin - 32);
		ts_reg = GPIO_OUT1_W1TS_REG;
		tc_reg = GPIO_OUT1_W1TC_REG;
		in_reg = GPIO_IN1_REG;
	}
	#endif
	return true;
}

// WARNING: This enables interrupts, do not call from an ISR!
inline void OptimizedGPIO::toggle()
{
	noInterrupts();
	read() ? reset() : set();
	interrupts();
}


/////////////////////////////////////////////////////////////////////
// ESP8266

#elif defined(ESP8266)

class OptimizedGPIO
{
	uint pin = PNUM_NOT_DEFINED;
	uint mask;
public:
	bool begin(uint pin, uint mode);
	bool read() { return (GPI & mask) != 0; }
	uint getPin() { return pin; }
	// do not use these with INPUT pins!
	void write(bool b) { b ? set() : reset(); }
	void set() { GPOS = mask; }
	void reset() { GPOC = mask; };
	void toggle();
};

bool OptimizedGPIO::begin(uint pin, uint mode)
{
	// pin 16 is not supported, it's used "to wake up the chip from deep sleep"
	if (pin >= 16)
		return false;
	this->pin = pin;
	pinMode(pin, mode);
	mask = 1 < pin;
	return true;
}

// WARNING: This enables interrupts, do not call from an ISR!
inline void OptimizedGPIO::toggle()
{
	noInterrupts();
	read() ? reset() : set();
	interrupts();
}


/////////////////////////////////////////////////////////////////////
// AVR, ATmega328, ATmega2560, etc

#elif defined(__AVR_ARCH__)

class OptimizedGPIO
{
	uint pin = PNUM_NOT_DEFINED;
	byte mask;
	volatile byte* outreg;
	volatile byte* inreg;
public:
	bool begin(uint pin, uint mode);
	bool read() { return (*inreg & mask) != 0; }
	uint getPin() { return pin; }
	// do not use these with INPUT pins!
	void write(bool b) { b ? set() : reset(); }
	void set();
	void reset();
	void toggle();
};

bool OptimizedGPIO::begin(uint pin, uint mode)
{
	byte port = digitalPinToPort(pin);
	if (port == NOT_A_PIN)
		return false;
	this->pin = pin;
	pinMode(pin, mode);
	mask = digitalPinToBitMask(pin);
	outreg = (volatile byte*)portOutputRegister(port);
	inreg = (volatile byte*)portInputRegister(port);
	return true;
}

inline void OptimizedGPIO::set()
{
	byte oldSREG = SREG;	// save interrupt state
	noInterrupts();			// disable all interrupts
	*outreg |= mask;
	SREG = oldSREG;			// restore interrupt state
}

inline void OptimizedGPIO::reset()
{
	byte oldSREG = SREG;
	noInterrupts();
	*outreg &= ~mask;
	SREG = oldSREG;
}

inline void OptimizedGPIO::toggle()
{
	byte oldSREG = SREG;
	noInterrupts();
	read() ? (*outreg &= ~mask) : (*outreg |= mask);
	SREG = oldSREG;
}


/////////////////////////////////////////////////////////////////////
// Special code for the dual-core Arduino GIGA

#elif defined(ARDUINO_GIGA)

#include <mbed.h>
#include <pinDefinitions.h>

class OptimizedGPIO
{
	uint pin = PNUM_NOT_DEFINED;
	mbed::DigitalInOut* gpio;
public:
	bool begin(uint pin, uint mode);
	bool read() { return gpio->read(); }
	uint getPin() { return pin; }
	// do not use these with INPUT pins!
	void write(bool b) { gpio->write(b); }
	void set() { gpio->write(1); }
	void reset() { gpio->write(0); }
	void toggle();
};

bool OptimizedGPIO::begin(uint pin, uint mode)
{
	if (pin >= PINS_COUNT)
		return false;
	this->pin = pin;
	pinMode(pin, mode);
	gpio = digitalPinToGpio(pin);
	return gpio != NULL;
}

inline void OptimizedGPIO::toggle()
{
	bool interruptsEnabled = !(__get_PRIMASK() & 1);
	noInterrupts();
	gpio->write(gpio->read() ^ 1);
	if (interruptsEnabled)
		interrupts();
}


/////////////////////////////////////////////////////////////////////
// Unknown microcontroller
// use the slow digitalRead() and digitalWrite()

#else 
#warning "Unknown microcontroller, needs OptimizedGPIO class"

class OptimizedGPIO
{
	uint pin = PNUM_NOT_DEFINED;
public:
	bool begin(uint pin, uint mode);
	bool read() { return digitalRead(pin); }
	uint getPin() { return pin; }
	// do not use these with INPUT pins!
	void write(bool b) { b ? set() : reset(); }
	void set() { digitalWrite(pin, 1); }
	void reset() { digitalWrite(pin, 0); }
	void toggle();
};

bool OptimizedGPIO::begin(uint pin, uint mode)
{
	this->pin = pin;
	pinMode(pin, mode);
	return true;
}

// NOTE: interrupts enabled, do not call from ISR
inline void OptimizedGPIO::toggle()
{
	noInterrupts();
	digitalWrite(pin, !digitalRead(pin));
	interrupts();
}

#endif
