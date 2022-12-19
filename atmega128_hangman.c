// NOTE: lines below added to allow compilation in simavr's build system
#undef F_CPU
#define F_CPU 16000000
#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "atmega128");

/*	Sample program for Olimex AVR-MT-128 with ATMega-128 processor
 *	Button 1 (Up)		- Turn ON the display
 *	Button 2 (Left)		- Set Text1 on the upline
 *	Button 3 (Middle)	- Hold to slide Text
 *	Button 4 (Right)	- Set Text2 on the downline
 *	Button 5 (Down)		- Turn OFF the display
 *	Compile with AVRStudio+WinAVR (gcc version 3.4.6)
 */

#define Text1 "Conflux"
#define Text2 "Rampard"

#include <string.h>
#include "avr/io.h"
#include "avr_lcd.h" // NOTE: changed header name to better fit in simavr's file naming conventions

#define __AVR_ATMEGA128__ 1

unsigned char data, Line = 0;
char Text[16], Ch;
unsigned int Bl = 1, LCD_State = 0, i, j;

void Delay(unsigned int b)
{
	volatile unsigned int a = b; // NOTE: volatile added to prevent the compiler to optimization the loop away
	while (a)
	{
		a--;
	}
}

/*****************************L C D**************************/

void E_Pulse()
{
	PORTC = PORTC | 0b00000100; // set E to high
	Delay(1400);				// delay ~110ms
	PORTC = PORTC & 0b11111011; // set E to low
}

void LCD_Init()
{
	// LCD initialization
	// step by step (from Gosho) - from DATASHEET

	PORTC = PORTC & 0b11111110;

	Delay(10000);

	PORTC = 0b00110000; // set D4, D5 port to 1
	E_Pulse();			// high->low to E port (pulse)
	Delay(1000);

	PORTC = 0b00110000; // set D4, D5 port to 1
	E_Pulse();			// high->low to E port (pulse)
	Delay(1000);

	PORTC = 0b00110000; // set D4, D5 port to 1
	E_Pulse();			// high->low to E port (pulse)
	Delay(1000);

	PORTC = 0b00100000; // set D4 to 0, D5 port to 1
	E_Pulse();			// high->low to E port (pulse)
}

void LCDSendCommand(unsigned char a)
{
	data = 0b00001111 | a;				 // get high 4 bits
	PORTC = (PORTC | 0b11110000) & data; // set D4-D7
	PORTC = PORTC & 0b11111110;			 // set RS port to 0
	E_Pulse();							 // pulse to set D4-D7 bits

	data = a << 4;						 // get low 4 bits
	PORTC = (PORTC & 0b00001111) | data; // set D4-D7
	PORTC = PORTC & 0b11111110;			 // set RS port to 0 -> display set to command mode
	E_Pulse();							 // pulse to set d4-d7 bits
}

void LCDSendChar(unsigned char a)
{
	data = 0b00001111 | a;				 // get high 4 bits
	PORTC = (PORTC | 0b11110000) & data; // set D4-D7
	PORTC = PORTC | 0b00000001;			 // set RS port to 1
	E_Pulse();							 // pulse to set D4-D7 bits

	data = a << 4;						 // get low 4 bits
	PORTC = (PORTC & 0b00001111) | data; // clear D4-D7
	PORTC = PORTC | 0b00000001;			 // set RS port to 1 -> display set to command mode
	E_Pulse();							 // pulse to set d4-d7 bits
}

void LCDSendTxt(char *a)
{
	int Temp;
	for (Temp = 0; Temp < strlen(a); Temp++)
	{
		LCDSendChar(a[Temp]);
	}
}

void LCDSendInt(long a)
{
	int C[20];
	unsigned char Temp = 0, NumLen = 0;
	if (a < 0)
	{
		LCDSendChar('-');
		a = -a;
	}
	do
	{
		Temp++;
		C[Temp] = a % 10;
		a = a / 10;
	} while (a);
	NumLen = Temp;
	for (Temp = NumLen; Temp > 0; Temp--)
		LCDSendChar(C[Temp] + 48);
}

void SmartUp(void)
{
	int Temp;
	for (Temp = 0; Temp < 1; Temp++)
		LCDSendCommand(CUR_UP);
}

void SmartDown(void)
{
	int Temp;
	for (Temp = 0; Temp < 40; Temp++)
		LCDSendCommand(CUR_DOWN);
}

void Light(short a)
{
	if (a == 1)
	{
		PORTC = PORTC | 0b00100000;
		DDRC = PORTC | 0b00100000;

		// IO0SET_bit.P0_25 = 1;
		// IO0DIR_bit.P0_25 = 1;
	}
	if (a == 0)
	{
		PORTC = PORTC & 0b11011111;
		DDRC = DDRC & 0b11011111;

		// IO0SET_bit.P0_25 = 0;
		// IO0DIR_bit.P0_25 = 0;
	}
}

/////////////////////////////////////////////////////////////////////

#define CG_RAM_ADDR 0x00000040
#define CHARMAP_SIZE 2

#define NAME0 "APPLE"
#define NAME1 "FUZZY"
#define NAME2 "AGENT"
#define NAME3 "BEACH"
#define NAME4 "AWARD"

static unsigned char CHARMAP[CHARMAP_SIZE][8] = {
	{0b00000, 0b00000, 0b01010, 0b10101, 0b10001, 0b01010, 0b00100, 0b00000},
	{0b00000, 0b00000, 0b10001, 0b01010, 0b00100, 0b01010, 0b10001, 0b00000}};

static char chosenWord[6];
static char chosenLetters[5];

void chars_init()
{
	for (int c = 0; c < CHARMAP_SIZE; c++)
	{
		LCDSendCommand(CG_RAM_ADDR + c * 8);
		for (int r = 0; r < 8; ++r)
			LCDSendChar(CHARMAP[c][r]);
	}
}

static void rnd_init()
{
	TCCR0 |= (1 << CS00); // Timer 0 no prescaling (@FCPU)
	TCNT0 = 0;			  // init counter
}

// generate a value between 0 and max
static int randomNumber(int max)
{
	return TCNT0 % max;
}

void chooseRandomWord()
{
	// generate random number between 0 and the number of words defined
	int randomnumber = randomNumber(5);
	if (randomnumber == 0)
	{
		strcpy(chosenWord, NAME0);
		// chosenWord = NAME0
	}

	else if (randomnumber == 1)
	{

		strcpy(chosenWord, NAME1);
	}

	else if (randomnumber == 2)
	{
		strcpy(chosenWord, NAME2);
	}

	else if (randomnumber == 3)
	{
		strcpy(chosenWord, NAME3);
	}

	else if (randomnumber == 4)
	{
		strcpy(chosenWord, NAME4);
	}
}

/*****************************L C D**************************/

void Port_Init()
{
	PORTA = 0b00011111;
	DDRA = 0b01000000; // NOTE: set A4-0 to initialize buttons to unpressed state
	PORTB = 0b00000000;
	DDRB = 0b00000000;
	PORTC = 0b00000000;
	DDRC = 0b11110111;
	PORTD = 0b11000000;
	DDRD = 0b00001000;
	PORTE = 0b00000000;
	DDRE = 0b00110000;
	PORTF = 0b00000000;
	DDRF = 0b00000000;
	PORTG = 0b00000000;
	DDRG = 0b00000000;
}

void clearRow(int line)
{
	if (line == 1)
		LCDSendCommand(DD_RAM_ADDR);
	else
		LCDSendCommand(DD_RAM_ADDR2);
	Line = line;
	for (i = 0; i < 16; i++)
		LCDSendChar(' ');
	if (line == 1)
		LCDSendCommand(DD_RAM_ADDR);
	else
		LCDSendCommand(DD_RAM_ADDR2);
}

static int maxColors = 1;
static int sequence[100];
static int cursor = 0;
static int points = 0;

//- for the 6 button, add the following
// above the main function add
static char currentChar = 'A';
static int gameStarted = 0;
static int lives = 3;

int main()
{

	LCDSendTxt(NAME0);

	Port_Init();
	LCD_Init();
	rnd_init();
	chars_init();
	// NOTE: added missing initialization steps
	LCDSendCommand(0x28);	  // function set: 4 bits interface, 2 display lines, 5x8 font
	LCDSendCommand(DISP_OFF); // display off, cursor off, blinking off
	LCDSendCommand(CLR_DISP); // clear display
	LCDSendCommand(0x06);	  // entry mode set: cursor increments, display does not shift

	LCDSendCommand(DISP_OFF);

	while (1)
	{

		// Value of Bl prevents holding the buttons
		// Bl = 0: One of the Buttons is pressed, release to press again
		// LCD_State value is the state of LCD: 1 - LCD is ON; 0 - LCD is OFF

		// Up Button (Button 8) : Turn ON Display
		if (!(PINA & 0b00000001) & Bl) // check state of button 1 and value of Bl and LCD_State
		{
			if (LCD_State)
			{
				if (currentChar < 'Z' && currentChar >= 'A')
				{
					currentChar++;
				}
				else
				{
					currentChar = 'A';
				}
				for (int i = 0; i < 15; i++)
				{
					Delay(999999999);
				}

				LCDSendCommand(DD_RAM_ADDR2);
				LCDSendChar(currentChar);
			}
			else
			{
				LCDSendCommand(DISP_ON);  // Turn ON Display
				LCDSendCommand(CLR_DISP); // Clear Display
				LCDSendTxt("welcome");	  // Print welcome screen
				Bl = 0;					  // Button is pressed
				LCD_State = 1;			  // Display is ON (LCD_State = 1)
			}
		}
		// Left Button (Button 4)
		if (!(PINA & 0b00000010) & Bl & LCD_State)
		{

			Bl = 0;
		}

		// Middle Button (Button 5) : Set Text1
		if (!(PINA & 0b00000100) & Bl & LCD_State) // check state of button 2 and value of Bl and LCD_State
		{
			if (!gameStarted)
			{
				LCDSendCommand(CLR_DISP);
				LCDSendCommand(DD_RAM_ADDR);
				chooseRandomWord();
				clearRow(1);
				LCDSendTxt("_____");
				LCDSendCommand(DD_RAM_ADDR + 13);
				for (int i = 0; i <= 2; i++)
				{
					LCDSendChar(0);
				}
				LCDSendCommand(DD_RAM_ADDR2);
				LCDSendChar('A');
				gameStarted = 1;
			}
			else
			{
				int found = 0;
				for (int i = 0; i < 5; i++)
				{

					if (currentChar == chosenWord[i] && chosenLetters[i] == 0)
					{
						chosenLetters[i] = currentChar;
						LCDSendCommand(DD_RAM_ADDR + i);
						LCDSendChar(currentChar);
						found = 1;
						points++;
					}
				}
				if (points == 5)
				{
					LCDSendCommand(CLR_DISP);
					LCDSendTxt("YOU WON!");
					LCDSendCommand(DD_RAM_ADDR2);
					LCDSendInt(lives);
					LCDSendTxt(" lives left!");
					gameStarted = 0;
					lives = 3;
					currentChar = 'A';
				}
				if (!found)
				{
					LCDSendCommand(DD_RAM_ADDR + 16 - lives);
					LCDSendChar(1);
					lives--;

					if (lives == 0)
					{
						LCDSendCommand(CLR_DISP);
						LCDSendTxt("GAME OVER!");
						gameStarted = 0;
						lives = 3;
						currentChar = 'A';
					}
				}
			}

			Bl = 0;	  // Button is pressed
			Line = 2; // text must be print on down line
		}

		// Right Button (Button 6)
		if (!(PINA & 0b00001000) & Bl & LCD_State) // check state of button 4 and value of Bl and LCD_State
		{
			Bl = 0;
		}

		// Down Button (Button 2)
		if (!(PINA & 0b00010000) & Bl & LCD_State) // check state of button 5 and value of Bl and LCD_State
		{
			if (currentChar <= 'Z' && currentChar > 'A')
			{
				currentChar--;
			}
			else
			{
				currentChar = 'Z';
			}
			for (int i = 0; i < 5; i++)
			{
				Delay(999999999);
			}

			LCDSendCommand(DD_RAM_ADDR2);
			LCDSendChar(currentChar);
			Bl = 0;
		}

		// check state of all buttons
		if (
			((PINA & 0b00000001) | (PINA & 0b00000010) | (PINA & 0b00000100) | (PINA & 0b00001000) | (PINA & 0b00010000)) == 31)
			Bl = 1; // if all buttons are released Bl gets value 1
	}
	return 0;
}
