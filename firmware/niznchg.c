/*
 * niznchg.c
 *
 * Created: 2014/9/21 21:17:06
 *  Author: whowe
 */ 

#define F_CPU 8000000

typedef unsigned char byte;

#include <avr/io.h>
#include <util/delay.h>

void init();
void output(byte pow);

int main(void)
{
	byte dir = 1;
	byte p = 0;
	
	init();
	
    while(1)
    {
		if (dir)
		{
	        if (p < 250)
			    p = p + 5;
			else
				dir = 0;
		}
		else
		{
			if (p > 10)
				p = p - 5;
			else
				dir = 1;
		}
		output(p);
		_delay_ms(50);
    }
}

void init()
{
	/* Clear Output */
	PORTB = 0x00;
	
    /*
    Port B Data Direction Register (controls the mode of all pins within port B)
    */
    DDRB = 1 << DDB2;

    /*
    Control Register A for Timer/Counter-0 (Timer/Counter-0 is configured using two registers: A and B)
    TCCR0A is 8 bits: [COM0A1:COM0A0:COM0B1:COM0B0:unused:unused:WGM01:WGM00]
    2<<COM0A0: sets bits COM0A0 and COM0A1, which (in Fast PWM mode) clears OC0A on compare-match, and sets OC0A at BOTTOM
    2<<COM0B0: sets bits COM0B0 and COM0B1, which (in Fast PWM mode) clears OC0B on compare-match, and sets OC0B at BOTTOM
    3<<WGM00: sets bits WGM00 and WGM01, which (when combined with WGM02 from TCCR0B below) enables Fast PWM mode
    */
    TCCR0A = 2 << COM0A0 | 3 << WGM00;
    
    /*
    Control Register B for Timer/Counter-0 (Timer/Counter-0 is configured using two registers: A and B)
    TCCR0B is 8 bits: [FOC0A:FOC0B:unused:unused:WGM02:CS02:CS01:CS00]
    0<<WGM02: bit WGM02 remains clear, which (when combined with WGM00 and WGM01 from TCCR0A above) enables Fast PWM mode
    1<<CS00: sets bits CS01 (leaving CS01 and CS02 clear), which tells Timer/Counter-0 to not use a prescalar
    */
    TCCR0B = 0 << WGM02 | 1 << CS00;
}


void output(byte pow)
{
    OCR0A = pow;
}