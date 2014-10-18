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
void init_adc();
void init_pwm();

void output_pwm(byte pow);

unsigned short read_adc(byte source);

#define IBAT  (1 << REFS0) | (0x0d << MUX0) /* External VRef, Differential PA1-PA2, Gain 20x */
#define VBAT  (1 << REFS0) | (2 << MUX0)    /* External VRef, Single-end PA2 */
#define TBAT  (1 << REFS0) | (3 << MUX0)    /* External VRef, Single-end PA3 */


unsigned short buf[3];

int main(void)
{
    byte dir = 1;
    byte p = 0;
    unsigned short ibat, vbat, tbat;
    
    init();
    
    while(1)
    {
        if (dir)
        {
            if (p < 254)
                p = p + 1;
            else
                dir = 0;
        }
        else
        {
            if (p >= 1)
                p = p - 1;
            else
            {
                _delay_ms(2000);
                dir = 1;
            }
                
        }
        output_pwm(p);
        _delay_ms(100);
        
        
        ibat = read_adc(IBAT);
        tbat = read_adc(TBAT);
        
        output_pwm(0);
        vbat = read_adc(VBAT);

        output_pwm(p);
        
        
        if (buf[0] != ibat)
            buf[0] = ibat;
        
        if (buf[1] != vbat)
            buf[1] = vbat;
            
        if (buf[2] != tbat)
            buf[2] = tbat;
    }
}

void init()
{
    init_pwm();
    init_adc();
}

void init_pwm()
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

void init_adc()
{
    PORTA = 0x00;
    DDRA = 0x00;
    
    /* ADC Enable, Prescaler factor 64 */ 
    ADCSRA = (1 << ADEN) | (7 << ADPS0);
    /* Left Adjusted */
    ADCSRB = (1 << ADLAR);
    /* Digital Buffer All Cleared */
    DIDR0 = 0xff;
    /* Initial Input VBAT*/
    ADMUX = VBAT;
}

unsigned short read_adc(byte source)
{
    ADMUX = source;
    
    /* Give up the first conversion */
    ADCSRA |= (1 << ADSC);
    loop_until_bit_is_clear(ADCSRA, ADSC);
    
    /* Do another conversion */
    ADCSRA |= (1 << ADSC);
    loop_until_bit_is_clear(ADCSRA, ADSC);
    
    return ADCL | (ADCH << 8);
}

void output_pwm(byte pow)
{
    OCR0A = pow;
}