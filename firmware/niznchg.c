/*
 * niznchg.c
 *
 * Created: 2014/9/21 21:17:06
 *  Author: whowe
 */ 

#define F_CPU 8000000

#include <avr/signature.h>
const char fusedata[] __attribute__ ((section (".fuse"))) =
{0xE2, 0xDF, 0xFF};
const char lockbits[] __attribute__ ((section (".lockbits"))) =
{0xFC};

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include <util/delay.h>

//#define CHG_FLASH 1   /* Flash while charge, for debug and time adjustment purpose */

typedef unsigned char byte;

#define NTC_3380 /* NTC Type can be 3950��3380 or 3435 */
#include "ntc.h"

void init();
void init_port();
void init_adc();
void init_pwm();

void output_pwm(unsigned short pow);

unsigned short read_adc(byte source);
signed char read_temp();

void led_on(byte led);
void led_off(byte led);

void welcome();
void detect_batt();
signed short charge_fast();
signed short charge_done();
void batt_alert(char mode);

#define IBAT  (1 << REFS0) | (0x0d << MUX0) /* External VRef, Differential PA1-PA2, Gain 20x */
#define VBAT  (1 << REFS0) | (2 << MUX0)    /* External VRef, Single-end PA2 */
#define TBAT  (1 << REFS0) | (3 << MUX0)    /* External VRef, Single-end PA3 */
#define IADJ  (1 << REFS0) | (7 << MUX0)    /* External VRef, Single-end PA7 */

#define RED     PORTB1 
#define GREEN   PORTB0


#define VOLT_VALID_LOW 0         /* V / 4.3 / 3.667 * 1023 */
#define VOLT_VALID_HIGH 970      /* V / 4.3 / 3.667 * 1023 */


#define TEMP_VALID_LOW -30
#define TEMP_VALID_HIGH 70

#define TEMP_FULL      49		/* Treated as Battery full */
#define TEMP_CRITICAL  60		/* Treated as Battery temp too high */

#define PHASE_1_DURATION (50 * 60)  /* In seconds */
#define CHG_TIMEOUT (60 * 60) /* In seconds */


#define CHG_FAST_CURRENT 470L    /* ���������ϵĵ�ѹֵ (mV)��Ҳ����Ŀ����� * 0.25 4.3 * 20 gain / 3.667v * 1023 */
#define CHG_SENSE_RATIO 1298L    /* �Ӽ�������ĵ�ѹֵת��Ϊ�ڲ� ADC ��ȡֵ��ϵ���� x = 1 / 4.3 * 20 / 3.667 * 1023 =  */

#ifndef CHG_CURRENT_ADJSUT
#define CHG_CURRENT_ADJSUT 0 /* �ǳ���Ҫ���۲���������ϵĵ�ѹ������̫������Ҫ�������ֵ (�����ٷֱ�) ��������� */
#endif

#define CHG_FULL 0
#define CHG_FUSE -1
#define CHG_TEMP -2
#define CHG_OTHER -3
#define CHG_BATT_FAIL -4
#define CHG_BATT_GONE -5

#define CHG_PWM_MIN 10
#define CHG_PWM_MAX 254

#define CHG_FAST_STOP_VOLTAGE 900   /* V / 4.3 / 3.667 * 1023 */
#define CHG_CURRENT_FUSE 1022

#define CHG_CURRENT_TOO_LOW 50   /* detects battery failure (break) */


int main(void)
{
#ifndef DIAGNOSE
    signed short ret;
#endif
    
    init();

    welcome();

#ifdef DIAGNOSE
	while (1)
	{
		led_on(RED);
		led_on(GREEN);
		output_pwm(128);
		_delay_ms(200);
		wdt_reset();
	}
#else
    while (1)
    {
        detect_batt();

        led_on(RED);
        led_off(GREEN);
        
        ret = charge_fast();
        led_off(RED);
        output_pwm(0);
        
        switch (ret)
        {
            case CHG_FULL:
                led_on(GREEN);
                break;
                
            case CHG_BATT_GONE:
                continue;
            
            case CHG_BATT_FAIL:
                batt_alert(0);
                break;

            case CHG_FUSE:
                batt_alert(1);
                continue;
                
            case CHG_TEMP:            
                batt_alert(1);
                continue;

            default:
                continue;
        }
       
        charge_done();
        
        _delay_ms(100);
    }
#endif

}

void init()
{
    init_port();
    init_pwm();
    init_adc();
    wdt_enable(WDTO_8S);
}

void init_port()
{
    /* Clear Output */
    PORTA = 0x00;
    PORTB = 0x00;
    
    /* PA[7..0] Input, PB2 as PWM Out, LEDs out */
    DDRA = 0x00;
    DDRB = 1 << DDB2 | 1 << RED | 1 << GREEN;
}

void init_pwm()
{
    /* Fast PWM */
    TCCR0A = 0 << COM0A0 | 3 << WGM00; /* Not to turn on right now */
    /* Fast PWM, Prescaler div1 */
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

void output_pwm(unsigned short pow)
{
    if (pow == 0)
    {
        TCCR0A &= ~(3 << COM0A0); /* Turn off pwm */
        OCR0A = 0;
    }
    else
    {
        TCCR0A |= 2 << COM0A0; /* Turn on pwm */
        OCR0A = pow - 1;
    }
}

void led_on(byte led)
{
    PORTB |= (1 << led);
}

void led_off(byte led)
{
    PORTB &= ~(1 << led);
}

void welcome()
{
    int i;
    for (i = 0; i < 5; i ++)
    {
        led_off(GREEN);
        led_on(RED);
        _delay_ms(100);

        led_off(RED);
        led_on(GREEN);
        _delay_ms(50);
        
        wdt_reset();
    }
    led_off(GREEN);
    led_off(RED);
}

void detect_batt()
{
    signed char tbat;
    short vbat;
    short tick = 0;
    short detect_count = 0;
    
    output_pwm(0);
    led_off(RED);

    while (1)
    {
        vbat = read_adc(VBAT) >> 6;
        tbat = read_temp();
        
        if ((vbat >= VOLT_VALID_LOW ) && (vbat <= VOLT_VALID_HIGH) && (tbat >= TEMP_VALID_LOW))
        {
            detect_count ++;
            
            if (detect_count >= 5)   /* detect battery valid 3 times in a row to avoid jitter */
            {
                led_off(GREEN);
                return;
            }                
        }
        else
        {
            detect_count = 0;
        }
        
        if ((tick & 0x07) == 0)
            led_on(GREEN);
        else
            led_off(GREEN);

        tick ++;
        _delay_ms(150);
        wdt_reset();
    }
}

#define TICK_PER_SECOND   304     /* This number should be adjusted to match the real time */

short calc_target_current()
{
    long cur = CHG_FAST_CURRENT * (100 + (CHG_CURRENT_ADJSUT)) / 100 * CHG_SENSE_RATIO / 1000;

#ifdef WITH_TRIM_POT
	short adj;

	adj = (read_adc(IADJ) >> 10); /* Range in [0..63] */

	if ((adj < 2) || (adj > 61))
	{
		adj = 32;
	}

	cur = (cur * (100 + adj - 32) / 100);  /* Adjust range: -32% to 32% */
#endif
	
    return cur;
}

signed short charge_fast()
{
    unsigned char pwm;
    short ibat, vbat;
    signed char tbat;
    short seconds = 0, tick = 0;
    short old_sec = 0;
    short target_current;

    pwm = CHG_PWM_MIN;

	target_current = calc_target_current();
	
    led_off(GREEN);
    led_on(RED);
    
    while (1)
    {
        output_pwm(pwm);
        
        ibat = (read_adc(IBAT) >> 6);
        ibat += (read_adc(IBAT) >> 6);
        ibat += (read_adc(IBAT) >> 6);
        ibat += (read_adc(IBAT) >> 6);

        ibat = ibat >> 2;

        /* Check if current is too high or out of control */
        if ((ibat >= CHG_CURRENT_FUSE) ||
            ((pwm == CHG_PWM_MIN) && (ibat > CHG_FAST_CURRENT)))
        {
            output_pwm(0);
            return CHG_FUSE; /* Finished because of Short Current */
        }

        /* if charge has started and current still too low, it means battery failure */
        if ((pwm == CHG_PWM_MAX) && (ibat <= CHG_CURRENT_TOO_LOW))
        {
            output_pwm(0);
            return CHG_BATT_FAIL;
        }
        
        /* Battery removal check by temperature */
        tbat = read_temp();
        if (tbat < TEMP_VALID_LOW)
        {
            output_pwm(0);
            return CHG_BATT_GONE;
        }

		/* Temperature check, if temp is too high */
		if (tbat > TEMP_CRITICAL)
		{
			output_pwm(0);  /* Reaching temperature level 2 is critical, battery is too hot */
			return CHG_TEMP;
		}

        /* Battery full check by temperature */
        if (tbat > TEMP_FULL)
        {
            output_pwm(0);
            return CHG_FULL; /* Reaching temperature level 1 is safe, it means battery full */
        }

        /* Constant current control */
        if (ibat < target_current)
        {
            if (pwm < CHG_PWM_MAX)
            {
                pwm ++;
            }
        }
        else if (ibat > target_current)
        {
            if (pwm > CHG_PWM_MIN)
            {
                pwm --;
            }
        }
        
        /* 1 Hour time out check */
        if (seconds > CHG_TIMEOUT) 
        {
            output_pwm(0);
            return CHG_FULL; /* Finish by Timeout */
        }

        if (old_sec != seconds)
        {
            old_sec = seconds;

			target_current = calc_target_current();
			
            wdt_reset();

#ifdef CHG_FLASH
            led_on(GREEN);
            _delay_ms(1);
            led_off(GREEN);
#endif    

            /* Phase two */
            if (seconds > PHASE_1_DURATION) /* Voltage check */
            {
                /* Disconnect power */
                output_pwm(0);
                
                /* Voltage measurement */
                vbat = (read_adc(VBAT) >> 6) + (read_adc(VBAT) >> 6) + (read_adc(VBAT) >> 6) + (read_adc(VBAT) >> 6);
                vbat = vbat >> 2;

                /* Resume power */
                output_pwm(pwm);
                
                /* Check if it reaches target voltage */
                if (vbat >= CHG_FAST_STOP_VOLTAGE)
                {
                    output_pwm(0);
                    return CHG_FULL; /* Finish at target voltage */
                }
            }
        }
        
        tick ++;
        if (tick > TICK_PER_SECOND)
        {
            tick -= TICK_PER_SECOND;
            seconds ++;  /* takes about 1 second (measured) */
        }
        _delay_ms(1);
    }
}

signed short charge_done()
{
    signed char tbat;
    unsigned t = 0;
    
    output_pwm(0);
    
    led_on(GREEN);
    led_off(RED);
    
    while (1)
    {
        tbat = read_temp();
        
        /* Battery removal detection */
        if (tbat < TEMP_VALID_LOW)
        {
            break;
        }
#if 0
		/* Temperature check, if temp is too high */
		if (tbat > TEMP_CRITICAL)
		{
			batt_alert(1);
		}
#endif
        t ++;
        _delay_ms(400);
        wdt_reset();
    }
    
    led_off(GREEN);
    return 0;
}

void batt_alert(char mode)
{
    signed char tbat;
    unsigned int tick = 0;

    output_pwm(0);
    
    while (1)
    {
        tbat = read_temp();

		/* Battery removal detection */        
        if (tbat < TEMP_VALID_LOW)
        {
            led_off(RED);
            break;
        }
        
        if ((tick & 0x01) == 0)
        {
            led_on(RED);
            led_off(GREEN);
        }            
        else
        {
            led_off(RED);
            if (mode)
            {
                led_on(GREEN);
            }                
        }            
            
        tick ++;
        _delay_ms(200);
        wdt_reset();
    }
}

