#undef F_CPU
#define F_CPU 16000000
#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "atmega128");
#include <util/delay.h>
#define __AVR_ATmega128__ 1
#include <avr/io.h>
#include <stdio.h>

// LCD HELPERS ---------------------------------------------------------------

#define CLR_DISP 0x00000001
#define DISP_ON 0x0000000C
#define DISP_OFF 0x00000008
#define CUR_HOME 0x00000002
#define CUR_OFF 0x0000000C
#define CUR_ON_UNDER 0x0000000E
#define CUR_ON_BLINK 0x0000000F
#define CUR_LEFT 0x00000010
#define CUR_RIGHT 0x00000014
#define CG_RAM_ADDR 0x00000040
#define DD_RAM_ADDR 0x00000080
#define DD_RAM_ADDR2 0x000000C0

static void lcd_delay(unsigned int b)
{
	volatile unsigned int a = b;
	while (a)
		a--;
}

static void lcd_pulse()
{
	PORTC = PORTC | 0b00000100; // set E to high
	lcd_delay(1400);			// delay ~110ms
	PORTC = PORTC & 0b11111011; // set E to low
}

static void lcd_send(int command, unsigned char a)
{
	unsigned char data;

	data = 0b00001111 | a;				 // get high 4 bits
	PORTC = (PORTC | 0b11110000) & data; // set D4-D7
	if (command)
		PORTC = PORTC & 0b11111110; // set RS port to 0 -> display set to command mode
	else
		PORTC = PORTC | 0b00000001; // set RS port to 1 -> display set to data mode
	lcd_pulse();					// pulse to set D4-D7 bits

	data = a << 4;						 // get low 4 bits
	PORTC = (PORTC & 0b00001111) | data; // set D4-D7
	if (command)
		PORTC = PORTC & 0b11111110; // set RS port to 0 -> display set to command mode
	else
		PORTC = PORTC | 0b00000001; // set RS port to 1 -> display set to data mode
	lcd_pulse();					// pulse to set d4-d7 bits
}

static void lcd_send_command(unsigned char a)
{
	lcd_send(1, a);
}

static void lcd_send_data(unsigned char a)
{
	lcd_send(0, a);
}

static void lcd_init()
{
	// LCD initialization
	// step by step (from Gosho) - from DATASHEET

	PORTC = PORTC & 0b11111110;

	lcd_delay(10000);

	PORTC = 0b00110000; // set D4, D5 port to 1
	lcd_pulse();		// high->low to E port (pulse)
	lcd_delay(1000);

	PORTC = 0b00110000; // set D4, D5 port to 1
	lcd_pulse();		// high->low to E port (pulse)
	lcd_delay(1000);

	PORTC = 0b00110000; // set D4, D5 port to 1
	lcd_pulse();		// high->low to E port (pulse)
	lcd_delay(1000);

	PORTC = 0b00100000; // set D4 to 0, D5 port to 1
	lcd_pulse();		// high->low to E port (pulse)

	lcd_send_command(0x28);		// function set: 4 bits interface, 2 display lines, 5x8 font
	lcd_send_command(DISP_OFF); // display off, cursor off, blinking off
	lcd_send_command(CLR_DISP); // clear display
	lcd_send_command(0x06);		// entry mode set: cursor increments, display does not shift

	lcd_send_command(DISP_ON);	// Turn ON Display
	lcd_send_command(CLR_DISP); // Clear Display
}

static void lcd_send_text(char *str)
{
	while (*str)
		lcd_send_data(*str++);
}

static void lcd_send_line1(char *str)
{
	lcd_send_command(DD_RAM_ADDR);
	lcd_send_text(str);
}

static void lcd_send_line2(char *str)
{
	lcd_send_command(DD_RAM_ADDR2);
	lcd_send_text(str);
}

#define DELAY(FREQ) (1000000L / FREQ / 2)

// Use a macro to generate functions for playing notes since _delay_us needs compile time constants
#define GENERATE_NOTE_FUNCTION(NOTE_NAME, FREQUENCY)             \
	static void play_##NOTE_NAME(unsigned long int ms, int half) \
	{                                                            \
		int i = 0;                                               \
		while (i * DELAY(FREQUENCY) < ms * 500)                  \
		{                                                        \
			PORTE = (PORTE & 0b11011111) | 0b00010000;           \
			_delay_us(DELAY(FREQUENCY));                         \
			if (half)                                            \
				PORTE = (PORTE || 0b00110000);                   \
			else                                                 \
				PORTE = (PORTE | 0b00100000) & 0b11101111;       \
			_delay_us(DELAY(FREQUENCY));                         \
			i++;                                                 \
		}                                                        \
	}

GENERATE_NOTE_FUNCTION(C4, 262)
GENERATE_NOTE_FUNCTION(D4, 294)
GENERATE_NOTE_FUNCTION(E4, 330)
GENERATE_NOTE_FUNCTION(F4, 349)
GENERATE_NOTE_FUNCTION(G4, 392)
GENERATE_NOTE_FUNCTION(A4, 440)
GENERATE_NOTE_FUNCTION(B4, 494)
GENERATE_NOTE_FUNCTION(C5, 523)

// GENERAL INIT - USED BY ALMOST EVERYTHING ----------------------------------

static void port_init()
{
	PORTA = 0b00011111;
	DDRA = 0b01000000; // buttons & led
	PORTB = 0b00000000;
	DDRB = 0b00000000;
	PORTC = 0b00000000;
	DDRC = 0b11110111; // lcd
	PORTD = 0b11000000;
	DDRD = 0b00001000;
	PORTE = 0b00100000;
	DDRE = 0b00110000; // buzzer
	PORTF = 0b00000000;
	DDRF = 0b00000000;
	PORTG = 0b00000000;
	DDRG = 0b00000000;
}

#define WHOLE (400)
#define HALF (200)

static void boci()
{
	lcd_send_line1("BO-        ");
	play_C4(HALF, 0);
	_delay_ms(HALF);

	lcd_send_line1("BOCI       ");
	play_E4(HALF, 0);
	_delay_ms(HALF);

	lcd_send_line1("BO-        ");
	play_C4(HALF, 0);
	_delay_ms(HALF);
	lcd_send_line1("BOCI       ");
	play_E4(HALF, 0);
	_delay_ms(HALF);

	lcd_send_line1("TAR-       ");
	play_G4(WHOLE, 0);
	_delay_ms(HALF);
	lcd_send_line1("TARKA      ");
	play_G4(WHOLE, 0);
	_delay_ms(WHOLE);

	lcd_send_line1("SE-        ");
	play_C4(HALF, 0);
	_delay_ms(HALF);

	lcd_send_line1("FU-        ");
	play_E4(HALF, 0);
	_delay_ms(HALF);

	lcd_send_line1("FULE       ");
	play_C4(HALF, 0);
	_delay_ms(HALF);
	lcd_send_line1("SE         ");
	play_E4(HALF, 0);
	_delay_ms(HALF);

	lcd_send_line1("FAR-       ");
	play_G4(WHOLE, 0);
	_delay_ms(HALF);
	lcd_send_line1("FARKA      ");
	play_G4(WHOLE, 0);
	_delay_ms(WHOLE);

	lcd_send_line1("O-         ");
	play_C5(HALF, 0);
	_delay_ms(HALF);
	lcd_send_line1("ODA        ");
	play_B4(HALF, 0);
	_delay_ms(HALF);
	lcd_send_line1("ME-        ");
	play_A4(HALF, 0);
	_delay_ms(HALF);
	lcd_send_line1("MEGYUNK    ");
	play_G4(HALF, 0);
	_delay_ms(HALF);
	lcd_send_line1("LAK-       ");
	play_F4(WHOLE, 0);
	_delay_ms(HALF);
	lcd_send_line1("LAKNI      ");
	play_A4(WHOLE, 0);
	_delay_ms(HALF);
	lcd_send_line1("A-         ");
	play_G4(HALF, 0);
	_delay_ms(HALF);
	lcd_send_line1("AHOL       ");
	play_F4(HALF, 0);
	_delay_ms(HALF);
	lcd_send_line1("TE-        ");
	play_E4(HALF, 0);
	_delay_ms(HALF);
	lcd_send_line1("TEJET      ");
	play_D4(HALF, 0);
	_delay_ms(HALF);
	lcd_send_line1("KAP-       ");
	play_C4(WHOLE, 0);
	_delay_ms(HALF);
	lcd_send_line1("KAPNI      ");
	play_C4(WHOLE, 0);
	_delay_ms(HALF);
	_delay_ms(WHOLE);
}

int main()
{
	port_init();
	lcd_init();
	while (1)
	{
		boci();
	}
}
