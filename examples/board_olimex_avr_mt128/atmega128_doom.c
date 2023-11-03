#undef F_CPU
#define F_CPU 16000000
#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "atmega128");
#include <util/delay.h>
#define	__AVR_ATmega128__	1
#include <avr/io.h>
#include <stdio.h>

// LCD HELPERS ---------------------------------------------------------------

#define		CLR_DISP	    0x00000001
#define		DISP_ON		    0x0000000C
#define		DISP_OFF	    0x00000008
#define		CUR_HOME      0x00000002
#define		CUR_OFF 	    0x0000000C
#define   CUR_ON_UNDER  0x0000000E
#define   CUR_ON_BLINK  0x0000000F
#define   CUR_LEFT      0x00000010
#define   CUR_RIGHT     0x00000014
#define   CG_RAM_ADDR		0x00000040
#define		DD_RAM_ADDR	  0x00000080
#define		DD_RAM_ADDR2	0x000000C0

static void lcd_delay(unsigned int b) {
	volatile unsigned int a = b;
	while (a)
		a--;
}

static void lcd_pulse() {
	PORTC = PORTC | 0b00000100;	//set E to high
	lcd_delay(1401); 			//delay ~110ms
	PORTC = PORTC & 0b11111011;	//set E to low
}

static void lcd_send(int command, unsigned char a) {
	unsigned char data;

	data = 0b00001111 | a;					//get high 4 bits
	PORTC = (PORTC | 0b11110000) & data;	//set D4-D7
	if (command)
		PORTC = PORTC & 0b11111110;			//set RS port to 0 -> display set to command mode
	else
		PORTC = PORTC | 0b00000001;			//set RS port to 1 -> display set to data mode
	lcd_pulse();							//pulse to set D4-D7 bits

	data = a<<4;							//get low 4 bits
	PORTC = (PORTC & 0b00001111) | data;	//set D4-D7
	if (command)
		PORTC = PORTC & 0b11111110;			//set RS port to 0 -> display set to command mode
	else
		PORTC = PORTC | 0b00000001;			//set RS port to 1 -> display set to data mode
	lcd_pulse();							//pulse to set d4-d7 bits
}

static void lcd_send_command(unsigned char a) {
	lcd_send(1, a);
}

static void lcd_send_data(unsigned char a) {
	lcd_send(0, a);
}

static void lcd_init() {
	//LCD initialization
	//step by step (from Gosho) - from DATASHEET

	PORTC = PORTC & 0b11111110;

	lcd_delay(10001);

	PORTC = 0b00110000;				//set D4, D5 port to 1
	lcd_pulse();					//high->low to E port (pulse)
	lcd_delay(1001);

	PORTC = 0b00110000;				//set D4, D5 port to 1
	lcd_pulse();					//high->low to E port (pulse)
	lcd_delay(1001);

	PORTC = 0b00110000;				//set D4, D5 port to 1
	lcd_pulse();					//high->low to E port (pulse)
	lcd_delay(1001);

	PORTC = 0b00100000;				//set D4 to 0, D5 port to 1
	lcd_pulse();					//high->low to E port (pulse)

	lcd_send_command(0x28); // function set: 4 bits interface, 2 display lines, 5x8 font
	lcd_send_command(DISP_OFF); // display off, cursor off, blinking off
	lcd_send_command(CLR_DISP); // clear display
	lcd_send_command(0x06); // entry mode set: cursor increments, display does not shift

	lcd_send_command(DISP_ON);		// Turn ON Display
	lcd_send_command(CLR_DISP);		// Clear Display
}

static void lcd_send_text(char *str) {
	while (*str)
		lcd_send_data(*str++);
}

static void lcd_send_line1(char *str) {
	lcd_send_command(DD_RAM_ADDR);
	lcd_send_text(str);
}

static void lcd_send_line2(char *str) {
	lcd_send_command(DD_RAM_ADDR2);
	lcd_send_text(str);
}

#define CHARMAP_SIZE 8
static unsigned char CHARMAP[CHARMAP_SIZE][8] = {
	{ 
        0b11110,
        0b11111,
        0b11111,
        0b11011,
        0b11011,
        0b11011,
        0b11011,
        0b11011,
    },
    { 
        0b01110,
        0b11111,
        0b11111,
        0b10011,
        0b10011,
        0b10011,
        0b10011,
        0b10011
    },
    { 
        0b01110,
        0b11111,
        0b11111,
        0b11001,
        0b11001,
        0b11001,
        0b11001,
        0b11001
    },
    { 
        0b11011,
        0b11111,
        0b11111,
        0b11111,
        0b11111,
        0b11111,
        0b11011,
        0b11011
    },
    { 
        0b11011,
        0b11011,
        0b11111,
        0b11110,
        0b11100,
        0b11000,
        0b10000,
        0b00000
    },
    { 
        0b10011,
        0b11011,
        0b11111,
        0b01110,
        0b00100,
        0b00000,
        0b00000,
        0b00000
    },
    { 
        0b11001,
        0b11011,
        0b11111,
        0b01110,
        0b00100,
        0b00000,
        0b00000,
        0b00000
    },
    { 
        0b11011,
        0b11011,
        0b11011,
        0b11011,
        0b11011,
        0b01011,
        0b00001,
        0b00000
    }					
	
};

static void chars_init() {
	for (int c = 0; c < CHARMAP_SIZE; ++c) {
		lcd_send_command(CG_RAM_ADDR + c*8);
		for (int r = 0; r < 8; ++r)
			lcd_send_data(CHARMAP[c][r]);
	}
}

#define DELAY(FREQ) (1000000L / FREQ / 2)

// Use a macro to generate functions for playing notes since _delay_us needs compile time constants
#define GENERATE_NOTE_FUNCTION(NOTE_NAME, FREQUENCY) \
static void play_##NOTE_NAME(unsigned long int ms, int half) { \
	int i = 0; \
	while (i * DELAY(FREQUENCY) < ms * 500) { \
        PORTE = (PORTE & 0b11011111) | 0b00010000;    \
        _delay_us(DELAY(FREQUENCY)); \
		if (half)\
        	PORTE = (PORTE | 0b00110001);    \
		else\
        	PORTE = (PORTE | 0b00100000) & 0b11101111;    \
        _delay_us(DELAY(FREQUENCY)); \
		i++; \
    } \
}

GENERATE_NOTE_FUNCTION(F1, 44)
GENERATE_NOTE_FUNCTION(Bb1, 58)
GENERATE_NOTE_FUNCTION(B1, 62)
GENERATE_NOTE_FUNCTION(C2, 65)
GENERATE_NOTE_FUNCTION(D2, 73)
GENERATE_NOTE_FUNCTION(E2, 82)
GENERATE_NOTE_FUNCTION(F2, 87)
GENERATE_NOTE_FUNCTION(G2, 98)
GENERATE_NOTE_FUNCTION(Bb2, 117)
GENERATE_NOTE_FUNCTION(B2, 123)
GENERATE_NOTE_FUNCTION(C3, 131)
GENERATE_NOTE_FUNCTION(D3, 147)
GENERATE_NOTE_FUNCTION(E3, 165)
GENERATE_NOTE_FUNCTION(G3, 196)
GENERATE_NOTE_FUNCTION(B3, 247)
GENERATE_NOTE_FUNCTION(D4, 294)
GENERATE_NOTE_FUNCTION(Eb4, 311)
GENERATE_NOTE_FUNCTION(E4, 330)
GENERATE_NOTE_FUNCTION(F4, 349)
GENERATE_NOTE_FUNCTION(G4, 392)
GENERATE_NOTE_FUNCTION(Ab4, 415)
GENERATE_NOTE_FUNCTION(A4, 440)
GENERATE_NOTE_FUNCTION(B4, 494)

// GENERAL INIT - USED BY ALMOST EVERYTHING ----------------------------------

static void port_init() {
	PORTA = 0b00011111;	DDRA = 0b01000000; // buttons & led
	PORTB = 0b00000000;	DDRB = 0b00000000;
	PORTC = 0b00000000;	DDRC = 0b11110111; // lcd
	PORTD = 0b11000000;	DDRD = 0b00001000;
	PORTE = 0b00100000;	DDRE = 0b00110000; // buzzer
	PORTF = 0b00000000;	DDRF = 0b00000000;
	PORTG = 0b00000000;	DDRG = 0b00000000;
}


#define WHOLE (250)
#define HALF (125)
#define QUARTER (67)
#define SIL (5)

int main() {
  port_init();
  lcd_init();
  chars_init();

  lcd_send_command(DD_RAM_ADDR + 6);
  lcd_send_data(0);
  lcd_send_data(1);
  lcd_send_data(2);
  lcd_send_data(3);
  lcd_send_command(DD_RAM_ADDR2 + 6);	
  lcd_send_data(4);
  lcd_send_data(SIL);
  lcd_send_data(6);
  lcd_send_data(7);
  while(1) {
    

    for (int l = 0; l < 2; l++) {
        
        play_F2(HALF,1);//
        _delay_ms(SIL);
        play_F2(HALF,1);
        _delay_ms(SIL);
        play_E3(HALF,1);
        _delay_ms(SIL);
        play_F2(HALF,1);
        _delay_ms(SIL);
        play_F2(HALF,1);
        _delay_ms(SIL);
        play_D3(HALF,1);
        _delay_ms(SIL);
        play_F2(HALF,1);
        _delay_ms(SIL);
        play_F2(HALF,1);
        _delay_ms(SIL);

        // ----

        play_C3(HALF,1);
        _delay_ms(SIL);
        play_F2(HALF,1);
        _delay_ms(SIL);
        play_F2(HALF,1);
        _delay_ms(SIL);
        play_Bb2(HALF,1);
        _delay_ms(SIL);
        play_F2(HALF,1);
        _delay_ms(SIL);
        play_F2(HALF,1);
        _delay_ms(SIL);
        play_B2(HALF,1);
        _delay_ms(SIL);
        play_C3(HALF,1);
        _delay_ms(SIL);

        // ----
        
        play_F2(HALF,1);
        _delay_ms(SIL);//
        play_F2(HALF,1);
        _delay_ms(SIL);
        play_E3(HALF,1);
        _delay_ms(SIL);
        play_F2(HALF,1);
        _delay_ms(SIL);
        play_F2(HALF,1);
        _delay_ms(SIL);
        play_D3(HALF,1);
        _delay_ms(SIL);
        play_F2(HALF,1);
        _delay_ms(SIL);
        play_F2(HALF,1);
        _delay_ms(SIL);

        // ----

        play_C3(HALF,1);
        _delay_ms(SIL);
        play_F2(HALF,1);
        _delay_ms(SIL);
        play_F2(HALF,1);
        _delay_ms(SIL);
        play_Bb2(275,1);//
        
    }

    for (int l = 0; l < 2; l++) {
        
        play_F1(HALF,1);
        _delay_ms(SIL);//
        play_F2(HALF,1);
        _delay_ms(SIL);
        play_E3(HALF,1);
        _delay_ms(SIL);
        play_F2(HALF,1);
        _delay_ms(SIL);
        play_F2(HALF,1);
        _delay_ms(SIL);
        play_D3(HALF,1);
        _delay_ms(SIL);
        play_F2(HALF,1);
        _delay_ms(SIL);
        play_F2(HALF,1);
        _delay_ms(SIL);

        // ----

        play_C3(HALF,1);
        _delay_ms(SIL);
        play_F2(HALF,1);
        _delay_ms(SIL);
        play_F2(HALF,1);
        _delay_ms(SIL);
        play_Bb2(HALF,1);
        _delay_ms(SIL);
        play_F2(HALF,1);
        _delay_ms(SIL);
        play_F2(HALF,1);
        _delay_ms(SIL);
        play_B2(HALF,1);
        _delay_ms(SIL);
        play_C3(HALF,1);
        _delay_ms(SIL);

        // ----
        
        play_F1(HALF,1);//
        _delay_ms(SIL);
        play_F2(HALF,1);
        _delay_ms(SIL);
        play_E3(HALF,1);
        _delay_ms(SIL);
        play_F2(HALF,1);
        _delay_ms(SIL);
        play_F2(HALF,1);
        _delay_ms(SIL);
        play_D3(HALF,1);
        _delay_ms(SIL);
        play_F2(HALF,1);
        _delay_ms(SIL);
        play_F2(HALF,1);
        _delay_ms(SIL);

        // ----

        play_C3(HALF,1);
        _delay_ms(SIL);
        play_F2(HALF,1);
        _delay_ms(SIL);
        play_F2(HALF,1);
        _delay_ms(SIL);
        play_Bb2(275,1);//
        _delay_ms(SIL);
        
    }

    play_F4(QUARTER,1);
    play_E4(QUARTER,1);
    play_Eb4(QUARTER,1);
    play_F4(QUARTER,1);
    play_Ab4(QUARTER,1);
    play_G4(QUARTER,1);
    play_F4(QUARTER,1);
    play_D4(QUARTER,1);
    play_F4(QUARTER,1);
    play_G4(QUARTER,1);
    play_A4(QUARTER,1);
    play_B4(QUARTER,1);
    play_A4(QUARTER,1);
    play_G4(QUARTER,1);
    play_F4(QUARTER,1);
    play_D4(QUARTER,1);

    // ----

    for (int l = 0; l < 2; l++) {
        
        play_F1(HALF,0);//
        _delay_ms(SIL);
        play_F1(HALF,0);
        _delay_ms(SIL);
        play_E2(HALF,0);
        _delay_ms(SIL);
        play_F1(HALF,0);
        _delay_ms(SIL);
        play_F1(HALF,0);
        _delay_ms(SIL);
        play_D2(HALF,0);
        _delay_ms(SIL);
        play_F1(HALF,0);
        _delay_ms(SIL);
        play_F1(HALF,0);
        _delay_ms(SIL);

        // ----

        play_C2(HALF,0);
        _delay_ms(SIL);
        play_F1(HALF,0);
        _delay_ms(SIL);
        play_F1(HALF,0);
        _delay_ms(SIL);
        play_Bb1(HALF,0);
        _delay_ms(SIL);
        play_F1(HALF,0);
        _delay_ms(SIL);
        play_F1(HALF,0);
        _delay_ms(SIL);
        play_B1(HALF,0);
        _delay_ms(SIL);
        play_C2(HALF,0);
        _delay_ms(SIL);

        // ----
        
        play_F1(HALF,0);//
        _delay_ms(SIL);
        play_F1(HALF,0);
        _delay_ms(SIL);
        play_E2(HALF,0);
        _delay_ms(SIL);
        play_F1(HALF,0);
        _delay_ms(SIL);
        play_F1(HALF,0);
        _delay_ms(SIL);
        play_D2(HALF,0);
        _delay_ms(SIL);
        play_F1(HALF,0);
        _delay_ms(SIL);
        play_F1(HALF,0);
        _delay_ms(SIL);

        // ----
        if (l == 0) {
            play_C2(HALF,0);
        _delay_ms(SIL);
            play_F1(HALF,0);
        _delay_ms(SIL);
            play_F1(HALF,0);
        _delay_ms(SIL);
            play_Bb1(HALF,0);//
        _delay_ms(SIL);
            play_F1(HALF,0);
        _delay_ms(SIL);
            play_F1(HALF,0);
        _delay_ms(SIL);
            play_F1(HALF,0);
        _delay_ms(SIL);
            play_F1(HALF,0);
        _delay_ms(SIL);
        }
        
    }

    play_D3(QUARTER,0);
    play_B2(QUARTER,0);
    play_G2(QUARTER,0);
    play_B2(QUARTER,0);
    play_D3(QUARTER,0);
    play_B2(QUARTER,0);
    play_D3(QUARTER,0);
    play_G3(QUARTER,0);
    play_D3(QUARTER,0);
    play_B2(QUARTER,0);
    play_D3(QUARTER,0);
    play_B2(QUARTER,0);
    play_D3(QUARTER,0);
    play_G3(QUARTER,0);
    play_B3(QUARTER,0);
    play_D4(QUARTER,0);

    for (int l = 0; l < 2; l++) {
        
        play_F1(HALF,0);//
        _delay_ms(SIL);
        play_F1(HALF,0);
        _delay_ms(SIL);
        play_E2(HALF,0);
        _delay_ms(SIL);
        play_F1(HALF,0);
        _delay_ms(SIL);
        play_F1(HALF,0);
        _delay_ms(SIL);
        play_D2(HALF,0);
        _delay_ms(SIL);
        play_F1(HALF,0);
        _delay_ms(SIL);
        play_F1(HALF,0);
        _delay_ms(SIL);

        // ----

        play_C2(HALF,0);
        _delay_ms(SIL);
        play_F1(HALF,0);
        _delay_ms(SIL);
        play_F1(HALF,0);
        _delay_ms(SIL);
        play_Bb1(HALF,0);
        _delay_ms(SIL);
        play_F1(HALF,0);
        _delay_ms(SIL);
        play_F1(HALF,0);
        _delay_ms(SIL);
        play_B1(HALF,0);
        _delay_ms(SIL);
        play_C2(HALF,0);
        _delay_ms(SIL);

        // ----
        
        play_F1(HALF,0);//
        _delay_ms(SIL);
        play_F1(HALF,0);
        _delay_ms(SIL);
        play_E2(HALF,0);
        _delay_ms(SIL);
        play_F1(HALF,0);
        _delay_ms(SIL);
        play_F1(HALF,0);
        _delay_ms(SIL);
        play_D2(HALF,0);
        _delay_ms(SIL);
        play_F1(HALF,0);
        _delay_ms(SIL);
        play_F1(HALF,0);
        _delay_ms(SIL);

        // ----
        if (l == 0) {
            play_C2(HALF,0);
            _delay_ms(SIL);
            play_F1(HALF,0);
            _delay_ms(SIL);
            play_F1(HALF,0);
            _delay_ms(SIL);
            play_Bb1(HALF,0);//
            _delay_ms(SIL);
            play_F1(HALF,0);
            _delay_ms(SIL);
            play_F1(HALF,0);
            _delay_ms(SIL);
            play_F1(HALF,0);
            _delay_ms(SIL);
            play_F1(HALF,0);
            _delay_ms(SIL);
        }
        
    }

    _delay_ms(2000);

  }
}
