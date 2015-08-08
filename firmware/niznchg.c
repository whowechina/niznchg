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

#define CHG_FLASH 1   /* Flash while charge, for debug and time adjustment purpose */

typedef unsigned char byte;

void init();
void init_port();
void init_adc();
void init_pwm();

void output_pwm(unsigned short pow);

unsigned short read_adc(byte source);

void led_on(byte led);
void led_off(byte led);

void detect_batt();
signed char charge();
void charge_done();
void batt_alert();

#define IBAT  (1 << REFS0) | (0x0d << MUX0) /* External VRef, Differential PA1-PA2, Gain 20x */
#define VBAT  (1 << REFS0) | (2 << MUX0)    /* External VRef, Single-end PA2 */
#define IADJ  (1 << REFS0) | (7 << MUX0)    /* External VRef, Single-end PA7 */

#define RED     PORTB1 
#define GREEN   PORTB0

#define CHG_CURRENT 80L    /* 检流电阻上的电压值 (mV)，也就是目标电流 * 0.25 */
#define CHG_SENSE_RATIO 1298L    /* 从检流电阻的电压值转换为内部 ADC 读取值的系数， x = 1 / 4.3 * 20 / 3.667 * 1023 =  */

#define CHG_FULL 0
#define CHG_FUSE -1
#define CHG_BATT_GONE -5

#define CHG_PWM_MIN 40
#define CHG_PWM_TEST 150
#define CHG_PWM_MAX 240

#define CHG_CURRENT_THRESHOLD 20
#define CHG_CURRENT_FUSE 500

#define BATT_VOLT_THRESHOLD 300
#define BATT_VOLT_FUSE 930

#define CHG_TIMEOUT (3600 * 4)

int main(void)
{
	signed char ret;
	
    init();

    while (1)
    {
        detect_batt();

        led_off(GREEN);
        led_on(RED);
		
        ret = charge();
		
		output_pwm(0);
		
        switch (ret)
        {
            case CHG_FULL:
                led_on(GREEN);
				charge_done();
                break;
            
            case CHG_FUSE:
                batt_alert();
                break;
                
            default:
                break;
        }
        
        _delay_ms(100);
    }
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

void detect_batt()
{
    short tick = 0;
    unsigned short ibat;
	
	led_off(RED);

    output_pwm(CHG_PWM_TEST);
   
    while (1)
    {
        ibat = read_adc(IBAT) >> 6;
        ibat += read_adc(IBAT) >> 6;
        ibat += read_adc(IBAT) >> 6;
        ibat += read_adc(IBAT) >> 6;
        
		ibat = ibat >> 2;
		
		if (ibat > CHG_CURRENT_THRESHOLD)
			return;

        if ((tick & 0x07) == 0)
            led_on(GREEN);
        else
            led_off(GREEN);

        _delay_ms(150);
        tick ++;
		
        wdt_reset();
    }
}

short calc_target_current()
{
	short adj;
    long cur = CHG_CURRENT * CHG_SENSE_RATIO / 1000;

	adj = (read_adc(IADJ) >> 10); /* Range in [0..63] */

	if ((adj < 2) || (adj > 61))
	{
		adj = 32;
	}

	cur = (cur * (100 + adj - 32) / 100);  /* Adjust range: -32% to 32% */
	
    return cur;
}

#define TICK_PER_SECOND   304     /* This number should be adjusted to match the real time */

signed char charge()
{
    unsigned char pwm;
    short ibat, vbat;
    short seconds = 0, tick = 0;
    short old_sec = 0;
    short target_current;

    pwm = CHG_PWM_TEST;

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
		
		vbat = (read_adc(VBAT) >> 6);
		vbat += (read_adc(VBAT) >> 6);
		vbat += (read_adc(VBAT) >> 6);
		vbat += (read_adc(VBAT) >> 6);
		
		vbat = vbat >> 2;

        /* Check if current is too high or out of control */
        if ((ibat >= CHG_CURRENT_FUSE) ||
            ((pwm == CHG_PWM_MIN) && (ibat > CHG_CURRENT)))
        {
            output_pwm(0);
            return CHG_FUSE; /* Finished because of Short Current */
        }

        /* Battery removal check*/
		if (ibat < CHG_CURRENT_THRESHOLD)
		{
			output_pwm(0);
			return CHG_BATT_GONE;
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
		
        /* Timeout and finish voltage check */
        if ((seconds > CHG_TIMEOUT) || (vbat > BATT_VOLT_FUSE))
        {
            output_pwm(0);
            return CHG_FULL; /* Finish */
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

void charge_done()
{
    unsigned t = 0;
    
    output_pwm(0);
    
    led_on(GREEN);
    led_off(RED);
    
    while (1)
    {
		/* Battery removal detection */
		if ((read_adc(VBAT) >> 6) < BATT_VOLT_THRESHOLD)
			return;
		
		/* Short current detection */
		if ((read_adc(IBAT) >> 6) > CHG_CURRENT_FUSE)
		{
			batt_alert();
			return;
		}

        _delay_ms(400);
        t ++;
		
        wdt_reset();
    }
    
    led_off(GREEN);
}

void batt_alert()
{
    byte tick = 0;

    output_pwm(0);
    
    while (1)
    {
		/* Battery removal detection */        
		if ((read_adc(VBAT) >> 6) < BATT_VOLT_THRESHOLD)
			return;
		
        if ((tick & 0x01) == 0)
        {
            led_on(RED);
            led_off(GREEN);
        }            
        else
        {
            led_off(RED);
            led_on(GREEN);
        }            
            
        _delay_ms(200);
        tick ++;
		
        wdt_reset();
    }
}

