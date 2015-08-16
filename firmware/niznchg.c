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

typedef unsigned char byte;

/* Set to 1 means flash while charge, useful for debug and time adjustment. */
const byte flash_while_charging = 1;   

void init();
void init_port();
void init_adc();
void init_pwm();

/* Hardware Controls */
void output_pwm(unsigned short pow);
unsigned short read_adc(byte source);
void led_on(byte led);
void led_off(byte led);

/* Charge Procedures */
void detect_batt();
byte charge();
void charge_done();
void batt_alert();

#define IBAT  (1 << REFS0) | (0x0d << MUX0) /* External VRef, Differential PA1-PA2, Gain 20x */
#define VBAT  (1 << REFS0) | (2 << MUX0)    /* External VRef, Single-end PA2 */
#define IADJ  (1 << REFS0) | (7 << MUX0)    /* External VRef, Single-end PA7 */

#define RED     PORTB1 
#define GREEN   PORTB0

#define CHG_CURRENT 80L    /* 检流电阻上的电压值 (mV)，也就是目标电流 * 0.25 */
#define CHG_SENSE_RATIO 1298L    /* 从检流电阻的电压值转换为内部 ADC 读取值的系数， x = 1 / 4.3 * 20 / 3.667 * 1023 =  */

#define CHG_DONE_FULL 0
#define CHG_ERR_FUSE 1
#define CHG_ERR_BATT_GONE 2

#define CHG_PWM_TEST 150    /* PWM value for battery existence test. */
#define CHG_PWM_MIN 40      /* Minimal PWM value allowed for charge. */
#define CHG_PWM_MAX 240     /* Maximum PWM value allowed for charge. */

#define CHG_CURRENT_THRESHOLD 20  /* Current exceeds THRESHOLD means battery exists. */
#define CHG_CURRENT_FUSE 500      /* Current exceeds FUSE means short-current happens. */

#define BATT_VOLT_THRESHOLD 600   /* Voltage below THRESHOLD means battery removed. */
#define BATT_VOLT_TARGET 905      /* Voltage reaching TARGET means charge is done. */

#define CHG_VOLT_DROP_THRESHOLD 880 /* Voltage higher than threshold when applying reverse detection. */
#define CHG_VOLT_DROP_DELTA     15  /* Voltage delta for reverse detection. */
#define CHG_VOLT_DROP_COUNT     5 

#define CHG_TIMEOUT 3600 * 4     /* in seconds */
#define ALERT_MINIMAL_TIME 10    /* in seconds */

int main(void)
{
    signed char ret;
    
    init();        /* Initialize everything including ADC, IO port, etc. */

    while (1)
    {
        detect_batt();   /* Until we see a battery connected */
        ret = charge();  /* Charge mode */
        output_pwm(0);   /* Charge done, make sure output is off */
        switch (ret)
        {
            case CHG_DONE_FULL:  /* If it is a normal finish,      */
                led_on(GREEN);   /* light on GREEN.             */
                charge_done();   /* Then wait for battery removal. */
                break;

            case CHG_ERR_FUSE:   /* If something wrong with the charge, */
                batt_alert();     /* alert and wait for battery removal. */
                break;
                
            default:
                break;
        }
        wdt_reset();
    }
}

void init()
{
    init_port();
    init_pwm();
    init_adc();
    wdt_enable(WDTO_4S);
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
    /* Fast PWM Mode, Prescaler set to 'div/1', initially off. */
    TCCR0A = 0 << COM0A0 | 3 << WGM00;
    TCCR0B = 0 << WGM02 | 1 << CS00;
}

void init_adc()
{
    ADCSRA = (1 << ADEN) | (7 << ADPS0); /* ADC Enable, Prescaler factor 64 */ 
    ADCSRB = (0 << ADLAR);               /* ADC result right-adjusted */
    DIDR0 = 0xff;        /* Digital Buffer All Cleared */
    ADMUX = VBAT;        /* Initial Input VBAT*/
}

unsigned short read_adc(byte source)
{
    unsigned short result = 0;
    int i;
    
    ADMUX = source;
    
    /* Give up the first conversion */
    ADCSRA |= (1 << ADSC);
    loop_until_bit_is_clear(ADCSRA, ADSC);
    
    /* Get average result from 4 conversions */
    for (i = 0; i < 4; i ++)
    {
        ADCSRA |= (1 << ADSC);
        loop_until_bit_is_clear(ADCSRA, ADSC);
        
        result += ADCL | (ADCH << 8);
    }
    
    return result >> 2;
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

    output_pwm(CHG_PWM_TEST);  /* Turn on output to detect battery. */
   
    while (1)
    {
        ibat = read_adc(IBAT);
        
        if (ibat > CHG_CURRENT_THRESHOLD)  /* Battery connected if we see current. */
            return;
            
        /* Flash the green LED indicating "Waiting for Battery" */
        (tick & 0x07) ? led_off(GREEN) : led_on(GREEN);

        _delay_ms(200);
        tick ++;
        
        wdt_reset();
    }
}

short calc_target_current()
{
    short adj;
    long cur = CHG_CURRENT * CHG_SENSE_RATIO / 1000;

    adj = (read_adc(IADJ) >> 4); /* Range in [0..63] */

    if ((adj < 2) || (adj > 61))
    {
        adj = 32;
    }

    cur = (cur * (100 + adj - 32) / 100);  /* Adjust range: -32% to 32% */
    
    return cur;
}

byte charge()
{
    byte pwm;
    short ibat, vbat, peak = 0;
    short seconds = 0, tick = 0;
    short old_sec = 0;
    short target_current;
    byte revcount = 0;

    pwm = CHG_PWM_TEST;

    target_current = calc_target_current();
    
    led_off(GREEN);
    led_on(RED);
    
    while (1)
    {
        output_pwm(pwm);
        
        ibat = read_adc(IBAT);
        vbat = read_adc(VBAT);

        /* Check if current is too high or out of control */
        if ((ibat >= CHG_CURRENT_FUSE) ||
            ((pwm == CHG_PWM_MIN) && (ibat > CHG_CURRENT)))
        {
            output_pwm(0); /* Turn off output as soon as possible */
            return CHG_ERR_FUSE; /* Finished because of Short Current */
        }

        /* Battery removal check*/
        if (ibat < CHG_CURRENT_THRESHOLD)
            return CHG_ERR_BATT_GONE;

        /* Constant current control */
        if ((ibat < target_current) && (pwm < CHG_PWM_MAX))
            pwm ++;

        if ((ibat > target_current) && (pwm > CHG_PWM_MIN))
            pwm --;
        
        /* Timeout and finish voltage check */
        if ((seconds > CHG_TIMEOUT) || (vbat > BATT_VOLT_TARGET))
            return CHG_DONE_FULL; /* Finish */

        /* Voltage reverse check when max voltage is higher than threshold. */
        if (peak > CHG_VOLT_DROP_THRESHOLD)
        {
            if (vbat < peak - CHG_VOLT_DROP_DELTA)
                revcount ++;
            else
                revcount = 0;
            
            if (revcount > CHG_VOLT_DROP_COUNT)
                return CHG_DONE_FULL;
        }
        
        /* Update the peak voltage, increase by 1 to avoid voltage jump. */
        if (peak < vbat)
            peak ++;

        if (old_sec != seconds)
        {
            old_sec = seconds;
            target_current = calc_target_current();
            wdt_reset();

            if (flash_while_charging)
                led_on(GREEN);
            _delay_ms(1);
            if (flash_while_charging)
                led_off(GREEN);
        }

#define TICK_PER_SECOND   304  /* Should be adjusted to match the real time */

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
    output_pwm(0);
    
    led_on(GREEN);
    led_off(RED);
    
    while (1)
    {
        /* Battery removal detection */
        if (read_adc(VBAT) < BATT_VOLT_THRESHOLD)
            return;
        
        /* Short current detection */
        if (read_adc(IBAT) > CHG_CURRENT_FUSE)
        {
            batt_alert();
            return;
        }

        _delay_ms(200);
        wdt_reset();
    }
    
    led_off(GREEN);
}

void batt_alert()
{
    byte tick = 0;
    byte can_quit = 0;
    
    output_pwm(0);
    
    while (1)
    {
        if (tick > ALERT_MINIMAL_TIME * 5)
            can_quit = 1;
        
        /* Battery removal detection */        
        if ((can_quit) && (read_adc(VBAT) < BATT_VOLT_THRESHOLD))
            return;
        
        led_on((tick & 0x01) ? RED : GREEN);
        led_off((tick & 0x01) ? GREEN : RED);
            
        _delay_ms(200);
        tick ++;
        
        wdt_reset();
    }
}

