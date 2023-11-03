/**
 * MiniTris -- Mini Tetris game
 * by Akos Kiss
 */

#undef F_CPU
#define F_CPU 16000000
#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "atmega128");

#define	__AVR_ATmega128__	1
#include <avr/io.h>


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

// TIMER-BASED RANDOM NUMBER GENERATOR ---------------------------------------

static void rnd_init() {
	TCCR0 |= (1  << CS00);	// Timer 0 no prescaling (@FCPU)
	TCNT0 = 0; 				// init counter
}

// generate a value between 0 and max
static int rnd_gen(int max) {
	return TCNT0 % max;
}

// SOUND GENERATOR -----------------------------------------------------------

typedef struct {
	int freq;
	int length;
} tune_t;

static tune_t TUNE_START[] = { { 2000, 40 }, { 0, 0 } };
static tune_t TUNE_LEVELUP[] = { { 3000, 20 }, { 0, 0 } };
static tune_t TUNE_GAMEOVER[] = { { 1000, 200 }, { 1500, 200 }, { 2000, 400 }, { 0, 0 } };

static void play_note(int freq, int len) {
	for (int l = 0; l < len; ++l) {
		volatile int i;
		PORTE = (PORTE & 0b11011111) | 0b00010000;	//set bit4 = 1; set bit5 = 0
		for (i=freq; i; i--);
		PORTE = (PORTE | 0b00100000) & 0b11101111;	//set bit4 = 0; set bit5 = 1
		for (i=freq; i; i--);
	}
}

static void play_tune(tune_t *tune) {
	while (tune->freq != 0) {
		play_note(tune->freq, tune->length);
		++tune;
	}
}

// BUTTON HANDLING -----------------------------------------------------------

#define BUTTON_NONE		0
#define BUTTON_CENTER	1
#define BUTTON_LEFT		2
#define BUTTON_RIGHT	3
#define BUTTON_UP		4
static int button_accept = 1;

static int button_pressed() {
	// right
	if (!(PINA & 0b00000001) & button_accept) { // check state of button 1 and value of button_accept
		button_accept = 0; // button is pressed
		return BUTTON_RIGHT;
	}

	// up
	if (!(PINA & 0b00000010) & button_accept) { // check state of button 2 and value of button_accept
		button_accept = 0; // button is pressed
		return BUTTON_UP;
	}

	// center
	if (!(PINA & 0b00000100) & button_accept) { // check state of button 3 and value of button_accept
		button_accept = 0; // button is pressed
		return BUTTON_CENTER;
	}

	// left
	if (!(PINA & 0b00010000) & button_accept) { // check state of button 5 and value of button_accept
		button_accept = 0; // button is pressed
		return BUTTON_LEFT;
	}

	return BUTTON_NONE;
}

static void button_unlock() {
	//check state of all buttons
	if (
		((PINA & 0b00000001)
		|(PINA & 0b00000010)
		|(PINA & 0b00000100)
		|(PINA & 0b00001000)
		|(PINA & 0b00010000)) == 31)
	button_accept = 1; //if all buttons are released button_accept gets value 1
}

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

//#define		ENTRY_INC	    0x00000007	//LCD increment
//#define		ENTRY_DEC	    0x00000005	//LCD decrement
//#define		SH_LCD_LEFT	  0x00000010	//LCD shift left
//#define		SH_LCD_RIGHT	0x00000014	//LCD shift right
//#define		MV_LCD_LEFT	  0x00000018	//LCD move left
//#define		MV_LCD_RIGHT	0x0000001C	//LCD move right

static void lcd_delay(unsigned int b) {
	volatile unsigned int a = b;
	while (a)
		a--;
}

static void lcd_pulse() {
	PORTC = PORTC | 0b00000100;	//set E to high
	lcd_delay(1400); 			//delay ~110ms
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

	lcd_delay(10000);

	PORTC = 0b00110000;				//set D4, D5 port to 1
	lcd_pulse();					//high->low to E port (pulse)
	lcd_delay(1000);

	PORTC = 0b00110000;				//set D4, D5 port to 1
	lcd_pulse();					//high->low to E port (pulse)
	lcd_delay(1000);

	PORTC = 0b00110000;				//set D4, D5 port to 1
	lcd_pulse();					//high->low to E port (pulse)
	lcd_delay(1000);

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

// SPEED LEVELS --------------------------------------------------------------

typedef struct {
	int	delay;
	int rows;
} level_t;

#define LEVEL_NUM 6
static level_t LEVELS[] = { { 5, 5 }, { 4, 10 }, { 3, 15 }, { 2, 20 }, { 1, 30 }, { 0, 0 } };
static int level_current = 0;
static int delay_cycle;

static void row_removed() {
	// do nothing if already at top speed
	if (level_current == LEVEL_NUM-1)
		return;

	// if enough rows removed, increase speed
	if (--LEVELS[level_current].rows == 0) {
		++level_current;
		play_tune(TUNE_LEVELUP);
	}
}

// PATTERNS AND PLAYFIELD ----------------------------------------------------
/* Vertical axis: 0 is top row, increments downwards
 * Horizontal axis: 0 is right column, increments leftwards */

#define PATTERN_NUM		4
#define PATTERN_SIZE	2
static unsigned char PATTERNS[PATTERN_NUM][PATTERN_SIZE] = { { 0b01, 0b00 }, { 0b01, 0b01 }, { 0b01, 0b11 }, { 0b11, 0b11 } };

static unsigned char current_pattern[PATTERN_SIZE];
static int current_row;
static int current_col;

// Actually, 1 row taller and 2 columns wider, which extras are filled with ones to help collision detection
#define PLAYFIELD_ROWS	16
#define PLAYFIELD_COLS	4
static unsigned char playfield[PLAYFIELD_ROWS + 1];

static void playfield_clear() {
	for (int r = 0; r < PLAYFIELD_ROWS; ++r)
		playfield[r] = 0b100001;
	playfield[PLAYFIELD_ROWS] = 0b111111;
}

static void merge_current_pattern_to_playfield() {
	// merge current piece to playfield
	for (int p = 0; p < PATTERN_SIZE; ++p)
		playfield[current_row + p] |= current_pattern[p] << (current_col + 1);
	// remove full lines and drop lines above
	for (int r = 0; r < PLAYFIELD_ROWS; ++r) {
		if (playfield[r] == 0b111111) {
			for (int rr = r; rr > 0; --rr)
				playfield[rr] = playfield[rr - 1];
			playfield[0] = 0b100001;
			row_removed(); // let's see whether we should increase the speed
		}
	}
}

static int collision(char *pattern, int row, int col) {
	int result = 0;
	for (int r = 0; r < PATTERN_SIZE; ++r)
		result |= playfield[row + r] & (pattern[r] << (col + 1));
	return !!result;
}

static void rotate_pattern(char *src_pattern, char *dst_pattern) {
	// rotate the piece
	dst_pattern[0] = (src_pattern[0] >> 1) | ((src_pattern[1] >> 1) << 1);
	dst_pattern[1] = (src_pattern[0] & 0x01) | ((src_pattern[1] & 0x01) << 1);
	// if the topmost row of the rotated piece is empty, shift the pattern upwards
	if (dst_pattern[0] == 0) {
		dst_pattern[0] = dst_pattern[1];
		dst_pattern[1] = 0;
	}
	// if the rightmost column of the rotated piece is empty, shift the pattern to the right
	if (((dst_pattern[0] & 0b01) == 0) && ((dst_pattern[1] & 0b01) == 0)) {
		dst_pattern[0] >>= 1;
		dst_pattern[1] >>= 1;
	}
}

// GRAPHICS ------------------------------------------------------------------

#define CHAR_EMPTY_PATTERN			0
#define CHAR_EMPTY_PLAYGROUND		1
#define CHAR_PATTERN_EMPTY			2
#define CHAR_PATTERN_PATTERN		3
#define CHAR_PATTERN_PLAYGROUND		4
#define CHAR_PLAYGROUND_EMPTY		5
#define CHAR_PLAYGROUND_PATTERN		6
#define CHAR_PLAYGROUND_PLAYGROUND	7
#define CHAR_EMPTY_EMPTY			' '
#define CHAR_ERROR					'X'

#define CHARMAP_SIZE 8
static unsigned char CHARMAP[CHARMAP_SIZE][8] = {
	{ 0b10101, 0b01010, 0b10101, 0b01010, 0, 0, 0, 0 },							// CHAR_EMPTY_PATTERN
	{ 0b11111, 0b11111, 0b11111, 0b11111, 0, 0, 0, 0 },							// CHAR_EMPTY_PLAYGROUND
	{ 0, 0, 0, 0, 0b10101, 0b01010, 0b10101, 0b01010 },							// CHAR_PATTERN_EMPTY
	{ 0b10101, 0b01010, 0b10101, 0b01010, 0b10101, 0b01010, 0b10101, 0b01010 },	// CHAR_PATTERN_PATTERN
	{ 0b11111, 0b11111, 0b11111, 0b11111, 0b10101, 0b01010, 0b10101, 0b01010 },	// CHAR_PATTERN_PLAYGROUND
	{ 0, 0, 0, 0, 0b11111, 0b11111, 0b11111, 0b11111 },							// CHAR_PLAYGROUND_EMPTY
	{ 0b10101, 0b01010, 0b10101, 0b01010, 0b11111, 0b11111, 0b11111, 0b11111 },	// CHAR_PLAYGROUND_PATTERN
	{ 0b11111, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111 }	// CHAR_PLAYGROUND_PLAYGROUND
};

static const unsigned char XLAT_PATTERN[] = { 0b0000, 0b0001, 0b0100, 0b0101 };
static const unsigned char XLAT_PLAYGROUND[] = { 0b0000, 0b0010, 0b1000, 0b1010 };
static const char XLAT_CHAR[] = {
	CHAR_EMPTY_EMPTY,			// 0b0000
	CHAR_EMPTY_PATTERN,			// 0b0001
	CHAR_EMPTY_PLAYGROUND,		// 0b0010
	CHAR_ERROR,					// 0b0011
	CHAR_PATTERN_EMPTY,			// 0b0100
	CHAR_PATTERN_PATTERN,		// 0b0101
	CHAR_PATTERN_PLAYGROUND,	// 0b0110
	CHAR_ERROR,					// 0b0111
	CHAR_PLAYGROUND_EMPTY,		// 0b1000
	CHAR_PLAYGROUND_PATTERN,	// 0b1001
	CHAR_PLAYGROUND_PLAYGROUND,	// 0b1010
	CHAR_ERROR,					// 0b1011
	CHAR_ERROR,					// 0b1100
	CHAR_ERROR,					// 0b1101
	CHAR_ERROR,					// 0b1110
	CHAR_ERROR					// 0b1111
};

static void chars_init() {
	for (int c = 0; c < CHARMAP_SIZE; ++c) {
		lcd_send_command(CG_RAM_ADDR + c*8);
		for (int r = 0; r < 8; ++r)
			lcd_send_data(CHARMAP[c][r]);
	}
}

static void screen_update() {
	lcd_send_command(DD_RAM_ADDR);		//set to Line 1

	for (int r1 = 0; r1 < PLAYFIELD_ROWS; ++r1) {
		unsigned char row = XLAT_PLAYGROUND[(playfield[r1] >> 1) & 0b11];
		for (int pr = 0; pr < PATTERN_SIZE; ++pr)
			if (r1 == current_row + pr)
				row |= XLAT_PATTERN[(current_pattern[pr] << current_col) & 0b11];
		lcd_send_data(XLAT_CHAR[row]);
	}

	lcd_send_command(DD_RAM_ADDR2);		//set to Line 2

	for (int r2 = 0; r2 < PLAYFIELD_ROWS; ++r2) {
		char row = XLAT_PLAYGROUND[(playfield[r2] >> 3) & 0b11];
		for (int pr = 0; pr < PATTERN_SIZE; ++pr)
			if (r2 == current_row + pr)
				row |= XLAT_PATTERN[((current_pattern[pr] << current_col) >> 2) & 0b11];
		lcd_send_data(XLAT_CHAR[row]);
	}
}

// THE GAME ==================================================================

int main() {
	port_init();
	lcd_init();
	chars_init();
	rnd_init();

	// "Splash screen"
	lcd_send_line1("    MiniTris");
	lcd_send_line2("    by Akiss");

	// loop of the whole program, always restarts game
	while (1) {
		int new_pattern;

		while (button_pressed() != BUTTON_CENTER) // wait till start signal
			button_unlock(); // keep on clearing button_accept

		playfield_clear(); // set up new playfield
		delay_cycle = 0; // start the timer
		new_pattern = 1; // start with new pattern
		play_tune(TUNE_START); // play a start signal

		// loop of the game
		while (1) {
			// start one new piece at the top
			if (new_pattern) {
				new_pattern = 0;

				// select a new random pattern
				int p = rnd_gen(PATTERN_NUM);
				for (int i = 0; i < PATTERN_SIZE; ++i)
					current_pattern[i] = PATTERNS[p][i];

				// rotate it randomly
				int r = rnd_gen(4);
				for (int j = 0; j < r; ++j) {
					char tmp_pattern[PATTERN_SIZE];
					rotate_pattern(current_pattern, tmp_pattern);
					for (int k = 0; k < PATTERN_SIZE; ++k)
						current_pattern[k] = tmp_pattern[k];
				}

				// place it randomly at the top of the playfield
				current_row = 0;
				current_col = rnd_gen(PLAYFIELD_COLS - PATTERN_SIZE + 1);

				// show the new piece on the screen
				screen_update();
			}

			// game over, if the new piece does not fit in the top row
			if ((current_row == 0) && collision(current_pattern, current_row, current_col))
				break;

			// if enough time passed, try to drop the current piece by one row
			if (++delay_cycle > LEVELS[level_current].delay) {
				delay_cycle = 0;
				// will not succeed at the last line, or if playfield below has conflicting elements
				if (collision(current_pattern, current_row + 1, current_col)) {
					// merge the current piece to the playfield then, and start with a new piece
					merge_current_pattern_to_playfield();
					new_pattern = 1;
					continue;
				}
				// otherwise, drop the piece by one row
				++current_row;
			}

			// if trying to move left or right, do so only if the piece does not leave or collide with the playfield
			int button = button_pressed();
			int horizontal = 0;
			if (button == BUTTON_LEFT)
				horizontal = +1;
			if (button == BUTTON_RIGHT)
				horizontal = -1;
			if ((horizontal != 0) && !collision(current_pattern, current_row, current_col + horizontal))
				current_col += horizontal;

			// if trying to rotate right, do only so if the piece does not collide with the playfield
			if (button == BUTTON_UP) {
				char tmp_pattern[PATTERN_SIZE];
				// rotate the current piece into a temp
				rotate_pattern(current_pattern, tmp_pattern);
				// check for collision, and make the rotation permanent if there is none
				if (!collision(tmp_pattern, current_row, current_col))
					for (int i = 0; i < PATTERN_SIZE; ++i)
						current_pattern[i] = tmp_pattern[i];
			}

			// once all movements are done, update the screen
			screen_update();

			// try to unlock the buttons (technicality but must be done)
			button_unlock();
		} // end of game-loop

		// playing some funeral tunes and displaying a game over screen
		play_tune(TUNE_GAMEOVER);
		lcd_send_line1("    GAME OVER   ");
		lcd_send_line2("Click to restart");

	} // end of program-loop, we never quit
}
