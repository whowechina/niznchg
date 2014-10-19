/*
 * niznchg.c
 *
 * Created: 2014/9/21 21:17:06
 *  Author: whowe
 */ 

#define F_CPU 8000000

typedef unsigned char byte;

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

#define NTC_3950 /* NTC Type can be 3950£¬3380 or 3435 */

#if defined(NTC_3950)
const signed char tref[256] PROGMEM =
{
    127, 127, 127, 127, 127, 127, 127, 127, 127, 124, 120, 116, 113, 109, 107, 104,
    101,  99,  97,  95,  93,  91,  90,  88,  86,  85,  84,  82,  81,  80,  78,  77,
     76,  75,  74,  73,  72,  71,  70,  69,  68,  67,  67,  66,  65,  64,  63,  63,
     62,  61,  61,  60,  59,  58,  58,  57,  57,  56,  55,  55,  54,  54,  53,  52,
     52,  51,  51,  50,  50,  49,  49,  48,  48,  47,  47,  46,  46,  45,  45,  44,
     44,  43,  43,  42,  42,  41,  41,  41,  40,  40,  39,  39,  38,  38,  38,  37,
     37,  36,  36,  36,  35,  35,  34,  34,  34,  33,  33,  32,  32,  32,  31,  31,
     31,  30,  30,  29,  29,  29,  28,  28,  28,  27,  27,  27,  26,  26,  26,  25,
     25,  24,  24,  24,  23,  23,  23,  22,  22,  22,  21,  21,  21,  20,  20,  20,
     19,  19,  19,  18,  18,  18,  17,  17,  16,  16,  16,  15,  15,  15,  14,  14,
     14,  13,  13,  13,  12,  12,  12,  11,  11,  11,  10,  10,   9,   9,   9,   8,
      8,   8,   7,   7,   7,   6,   6,   5,   5,   5,   4,   4,   3,   3,   3,   2,
      2,   1,   1,   1,   0,   0,  -1,  -1,  -1,  -2,  -2,  -3,  -3,  -4,  -4,  -5,
     -5,  -6,  -6,  -7,  -7,  -8,  -8,  -9,  -9, -10, -10, -11, -11, -12, -13, -13,
    -14, -14, -15, -16, -16, -17, -18, -19, -19, -20, -21, -22, -23, -24, -25, -26,
    -27, -28, -29, -30, -32, -33, -35, -36, -38, -40, -43, -46, -50, -55, -63, -127
};
#elif defined(NTC_3380)
const signed char tref[256] PROGMEM =
{
    127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 125, 122, 
    118, 115, 113, 110, 108, 106, 103, 101,  99,  98,  96,  94,  92,  91,  89,  88,
     87,  85,  84,  83,  81,  80,  79,  78,  77,  76,  75,  74,  73,  72,  71,  70,
     69,  68,  67,  67,  66,  65,  64,  63,  63,  62,  61,  60,  60,  59,  58,  58,
     57,  56,  56,  55,  54,  54,  53,  52,  52,  51,  51,  50,  49,  49,  48,  48,
     47,  47,  46,  45,  45,  44,  44,  43,  43,  42,  42,  41,  41,  40,  40,  39,
     39,  38,  38,  37,  37,  37,  36,  36,  35,  35,  34,  34,  33,  33,  32,  32,
     32,  31,  31,  30,  30,  29,  29,  29,  28,  28,  27,  27,  26,  26,  26,  25,
     25,  24,  24,  24,  23,  23,  22,  22,  22,  21,  21,  20,  20,  20,  19,  19,
     18,  18,  18,  17,  17,  16,  16,  16,  15,  15,  14,  14,  14,  13,  13,  12,
     12,  11,  11,  11,  10,  10,   9,   9,   9,   8,   8,   7,   7,   7,   6,   6,
      5,  5,    4,   4,   4,   3,   3,   2,   2,   1,   1,   1,   0,   0,  -1,  -1,
     -2,  -2,  -3,  -3,  -4,  -4,  -5,  -5,  -5,  -6,  -6,  -7,  -7,  -8,  -9,  -9,
    -10, -10, -11, -11, -12, -12, -13, -14, -14, -15, -15, -16, -17, -17, -18, -19,
    -19, -20, -21, -21, -22, -23, -24, -25, -25, -26, -27, -28, -29, -30, -31, -32,
    -34, -35, -36, -38, -39, -41, -42, -44, -46, -49, -51, -55, -59, -64, -73, -127
};
#elif defined(NTC_3435)
const signed char tref[256] PROGMEM =
{
    127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 126, 123, 119,
    116, 114, 111, 108, 106, 104, 102, 100,  98,  96,  94,  93,  91,  90,  88,  87,
     85,  84,  83,  82,  80,  79,  78,  77,  76,  75,  74,  73,  72,  71,  70,  69,
     68,  67,  67,  66,  65,  64,  63,  63,  62,  61,  60,  60,  59,  58,  58,  57,
     56,  56,  55,  54,  54,  53,  52,  52,  51,  51,  50,  50,  49,  48,  48,  47,
     47,  46,  46,  45,  45,  44,  44,  43,  43,  42,  42,  41,  41,  40,  40,  39,
     39,  38,  38,  37,  37,  36,  36,  35,  35,  35,  34,  34,  33,  33,  32,  32,
     31,  31,  31,  30,  30,  29,  29,  28,  28,  28,  27,  27,  26,  26,  26,  25,
     25,  24,  24,  24,  23,  23,  22,  22,  22,  21,  21,  20,  20,  20,  19,  19,
     18,  18,  18,  17,  17,  16,  16,  16,  15,  15,  14,  14,  14,  13,  13,  12,
     12,  12,  11,  11,  10,  10,  10,   9,   9,   8,   8,   8,   7,   7,   6,   6,
      6,   5,   5,   4,   4,   4,   3,   3,   2,   2,   1,   1,   0,   0,   0,  -1,
     -1,  -2,  -2,  -3,  -3,  -4,  -4,  -5,  -5,  -6,  -6,  -7,  -7,  -8,  -8,  -9,  
     -9, -10, -10, -11, -11, -12, -12, -13, -14, -14, -15, -15, -16, -17, -17, -18,
    -19, -19, -20, -21, -22, -22, -23, -24, -25, -26, -27, -28, -28, -30, -31, -32,
    -33, -34, -35, -37, -38, -40, -42, -43, -45, -48, -51, -54, -58, -63, -72, -127
};

#endif

void init();
void init_port();
void init_adc();
void init_pwm();

void output_pwm(byte pow);

unsigned short read_adc(byte source);
signed char read_temp();

void detect_batt();
signed char charge_fast();
signed char charge_trickle();

#define IBAT  (1 << REFS0) | (0x0d << MUX0) /* External VRef, Differential PA1-PA2, Gain 20x */
#define VBAT  (1 << REFS0) | (2 << MUX0)    /* External VRef, Single-end PA2 */
#define TBAT  (1 << REFS0) | (3 << MUX0)    /* External VRef, Single-end PA3 */


unsigned short buf[3];

int main(void)
{
    init();
    
    while (1)
    {
        detect_batt();
        charge_fast();
        charge_trickle();
        _delay_ms(10000);
    }
}

void init()
{
    init_port();
    init_pwm();
    init_adc();
}

void init_port()
{
    /* Clear Output */
    PORTA = 0x00;
    PORTB = 0x00;
    
    /* PA[7..0] Input, PB2 as PWM Out */
    DDRA = 0x00;
    DDRB = 1 << DDB2;
}
void init_pwm()
{
    /* Fast PWM */
    TCCR0A = 2 << COM0A0 | 3 << WGM00;
    /* Fast PWM, No Prescalar */
    TCCR0B = 0 << WGM02 | 1 << CS00;
}

void init_adc()
{
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


signed char read_temp()
{
    unsigned char tbat;
    
    tbat = read_adc(TBAT) >> 8;
    
    return pgm_read_byte(tref + tbat);
}

void output_pwm(byte pow)
{
    OCR0A = pow;
}

#define BATT_VALID_LOW 231
#define BATT_VALID_HIGH 912
#define TEMP_VALID_LOW -20
#define TEMP_VALID_HIGH 50

void detect_batt()
{
    signed char tbat;
    short vbat;
    
    output_pwm(0);

    while (1)
    {
        vbat = read_adc(VBAT) >> 6;
        tbat = read_temp();
        
        if ((vbat >= BATT_VALID_LOW ) && (vbat <= BATT_VALID_HIGH)
            && (tbat >= TEMP_VALID_LOW ) && (tbat <= TEMP_VALID_HIGH))
        {
            return;
        }
        _delay_ms(500);
    }
}

#define CHG_FAST_CURRENT 480
#define CHG_FAST_PWM_MIN 40
#define CHG_FAST_PWM_MAX 254

#define CHG_FAST_STOP_VOLTAGE 900
#define CHG_CURRENT_FUSE 1000

signed char charge_fast()
{
    unsigned char pwm;
    short ibat, vbat;
    signed char tbat;
    unsigned short tick = 0;

    pwm = CHG_FAST_PWM_MIN;
        
    while (1)
    {
        output_pwm(pwm);
        ibat = read_adc(IBAT) >> 6;
        if (ibat >= CHG_CURRENT_FUSE)
        {
            output_pwm(0);
            return -1; /* Short Current */
        }
        
        if (ibat < CHG_FAST_CURRENT)
        {
            if (pwm < CHG_FAST_PWM_MAX)
            {
                pwm ++;
            }
        }
        else if (ibat > CHG_FAST_CURRENT)
        {
            if (pwm > CHG_FAST_PWM_MIN)
            {
                pwm --;
            }
        }
        
        if ((tick & 0x3ff) == 0x3ff) /* approximately 1.1 second */
        {
            output_pwm(0);
            _delay_ms(10);
            vbat = read_adc(VBAT) >> 6;
            tbat = read_temp();
            if ((tbat < TEMP_VALID_LOW) || (tbat > TEMP_VALID_HIGH))
            {
                output_pwm(0);
                return -2; /* Temperature Fault */
            }
            if (vbat >= CHG_FAST_STOP_VOLTAGE)
            {
                output_pwm(0);
                return vbat + vbat; /* Finish at Target Voltage */
            }
        }
        
        tick ++;
        _delay_ms(1);
    }
}

signed char charge_trickle()
{
    signed char tbat;
    short vbat;
    
    /* Wait Until Battery Removed */
    output_pwm(0);

    while (1)
    {
        vbat = read_adc(VBAT) >> 6;
        tbat = read_temp();
        
        if ((vbat < BATT_VALID_LOW ) || (vbat > BATT_VALID_HIGH)
        || (tbat < TEMP_VALID_LOW ) || (tbat > TEMP_VALID_HIGH))
        {
            return 0;
        }
        _delay_ms(500);
    }
        
}