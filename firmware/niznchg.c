/*
 * niznchg.c
 *
 * Created: 2015/10/1
 *  Author: whowe
 */ 

#define F_CPU 9600000

#include <avr/signature.h>
const char fusedata[] __attribute__ ((section (".fuse"))) =
{0x7A, 0xFE};
const char lockbits[] __attribute__ ((section (".lockbits"))) =
{0xFC};

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include <util/delay.h>

typedef unsigned char byte;

static void init();

/* Hardware Controls */
static void output_pwm(byte pow);
static unsigned short read_adc(byte source, byte n);

/* Charge Procedures */
static void welcome();
static void detect_batt();
static byte charge();
static void charge_done();
static void batt_alert(byte type);

#define IBAT  (0 << REFS0) | (0 << ADLAR) | (1 << MUX0) /* Vcc as VRef, Single-ended ADC1 (PB2) */
#define VBAT  (0 << REFS0) | (0 << ADLAR) | (2 << MUX0) /* Vcc as VRef, Single-ended ADC2 (PB4) */
#define TBAT  (0 << REFS0) | (0 << ADLAR) | (3 << MUX0) /* Vcc as VRef, Single-ended ADC3 (PB3) */

#define GREEN PORTB1
#define RED PORTB5

#define LED_ON(led) (PORTB |= (1 << led))
#define LED_OFF(led) (PORTB &= ~(1 << led))

#define CHG_DONE_FULL 0
#define CHG_ERR_FUSE 1
#define CHG_ERR_LOWCURRENT 2
#define CHG_ERR_BATT_GONE 3

#define CHG_PWM_MIN 1       /* Minimal PWM value allowed for charge. */
#define CHG_PWM_MAX 240     /* Maximum PWM value allowed for charge. */

#define CHG_PWM_FREQ_TRIG_LOW 16 
#define CHG_PWM_FREQ_TRIG_HIGH 32

#define CHG_CURRENT 29L    /* (Target current = 0.3A) * (Sense resistor = 0.5ohm) / (VRef = 5V) * (Scale = 1024) */
#define CHG_CURRENT_THRESHOLD 4   /* Current exceeds THRESHOLD means battery exists. */
#define CHG_CURRENT_MAX 50        /* Current exceeds MAX with PWM_MIN is out of control. */

#define BATT_VOLT_TARGET 930         /* Voltage reaching TARGET means charge is done. */

#define CHG_VOLT_DROP_DELTA     8     /* Voltage delta for reverse detection. */
#define CHG_VOLT_DROP_COUNT     5     /* How many times in a row we consider as real reverse */

#define TBAT_VALID_HIGH      940  /* TBAT higher than this means battery not connected */

/* Timeout controls, phase 1 is forced charging and phase 2 is with voltage drop detection */
#define CHG_TIMEOUT (3600 * 4)   /* Timeout (in seconds) of entire charging process */
#define CHG_TIME_FORCE 3600      /* Timeout (in seconds) of forced charging (phase 1) */

int main(void)
{
    signed char ret;
    
    init();   /* Initialize everything */

    welcome();
    
    while (1)
    {
        detect_batt();   /* Until we see a battery connected */
        ret = charge();  /* Charge mode */
        output_pwm(0);   /* Charge done, make sure output is off */
        switch (ret)
        {
            case CHG_DONE_FULL:  /* If it is a normal finish,      */
                charge_done();   /* Then wait for battery removal. */
                break;

            case CHG_ERR_LOWCURRENT:   /* If something wrong with the charge, */
                batt_alert(0);     /* alert and wait for battery removal. */
                break;
                
            case CHG_ERR_FUSE:   /* If something wrong with the charge, */
                batt_alert(1);     /* alert and wait for battery removal. */
                break;
                
            default:
                break;
        }
        wdt_reset();
    }
}

void init()
{
    /* Port Clear Output */
    PORTB = 0x00;
    /* PB0 as PWM Out, LED out */
    DDRB = 1 << DDB0 | 1 << GREEN | 1 << RED;

    /* Fast PWM Mode, Prescaler set to 'div/1', initially off. */
    TCCR0A = 0 << COM0A0 | 3 << WGM00;
    TCCR0B = 0 << WGM02 | 2 << CS00;
    
    /* Initialize ADC */
    ADCSRA = (1 << ADEN) | (7 << ADPS0); /* ADC Enable, Prescaler factor 128 */
    DIDR0 = 0xff;        /* Digital Buffer All Cleared */
    ADMUX = VBAT;        /* Initial Input VBAT*/

    wdt_enable(WDTO_8S);
}

unsigned short read_adc(byte source, byte n)
{
    unsigned short result = 0;
    byte i;
    
    ADMUX = source;
    
    /* Give up the first AD conversion sample */
    ADCSRA |= (1 << ADSC);
    loop_until_bit_is_clear(ADCSRA, ADSC);
    
    /* Get average result from 2^n samples */
    for (i = 0; i < (1 << n); i ++)
    {
        ADCSRA |= (1 << ADSC);
        loop_until_bit_is_clear(ADCSRA, ADSC);
        
        result += ADCL | (ADCH << 8);
    }
    return result >> n;
}

void output_pwm(byte pow)
{
    if (pow <= CHG_PWM_FREQ_TRIG_LOW)
        TCCR0B = 0 << WGM02 | 3 << CS00;  /* 9.6MHz/256, div 8, 4.69KHz*/
    
    if (pow >= CHG_PWM_FREQ_TRIG_HIGH)
        TCCR0B = 0 << WGM02 | 2 << CS00;  /* 9.6MHz/256, Div 64, 586Hz */
        
    if (pow == 0)
    {
        TCCR0A &= ~(3 << COM0A0); /* Turn off pwm */
        PORTB &= ~(1 << PORTB0);
    }
    else if (pow == 255)
    {
        TCCR0A &= ~(3 << COM0A0); /* Turn off pwm */
        PORTB |= (1 << PORTB0); 
    }
    else
    {
        TCCR0A |= 2 << COM0A0; /* Turn on pwm */
        OCR0A = pow;
    }
}

void welcome()
{
    int i;
    for (i = 0; i < 5; i ++)
    {
        LED_OFF(GREEN);
        LED_ON(RED);
        _delay_ms(100);

        LED_OFF(RED);
        LED_ON(GREEN);
        _delay_ms(50);

        wdt_reset();
    }
}


void detect_batt()
{
    short tick = 0;
    unsigned short tbat;
    
    output_pwm(0);
    LED_OFF(RED);
    
    while (1)
    {
        
        tbat = read_adc(TBAT, 3);
        if (tbat < TBAT_VALID_HIGH)  /* If battery connected  */
            return;
            
        /* Flash the green LED indicating "Waiting for Battery" */
        (tick % 18) ? LED_OFF(GREEN) : LED_ON(GREEN);

        _delay_ms(110);
        tick ++;
        
        wdt_reset();
    }
}

byte charge()
{
    byte pwm;
    short ibat, vbat, tbat, real_vbat, peak;
    short seconds = 0, tick = 0;
    short old_sec = 0;
    short target_current;
    byte revcount = 0;

    pwm = CHG_PWM_MIN;
    target_current = CHG_CURRENT;
    peak = 0;
    
    LED_ON(RED);
    LED_OFF(GREEN);

    while (1)
    {
        output_pwm(pwm);
        
        ibat = read_adc(IBAT, 3);
        vbat = read_adc(VBAT, 2);

        /* Check if current is too high or out of control, skip the first second */
        if (seconds >= 1)
        {
            if ((pwm == CHG_PWM_MIN) && (ibat > CHG_CURRENT_MAX))
            {
                output_pwm(0); /* Turn off output as soon as possible */
                return CHG_ERR_FUSE; /* Finished because of Short Current */
            }
        }
                
        /* Battery removal check*/
        if ((pwm == CHG_PWM_MAX) && (ibat < CHG_CURRENT_THRESHOLD))
            return CHG_ERR_LOWCURRENT;

        /* Constant current control */
        if ((ibat < target_current) && (pwm < CHG_PWM_MAX))
            pwm ++;

        if ((ibat > target_current) && (pwm > CHG_PWM_MIN))
            pwm --;
        
        /* Remove the voltage on i-sense resistor to get real voltage */
        real_vbat = vbat - (ibat / 4);
        
        /* Timeout and finish voltage check */
        if (seconds > CHG_TIMEOUT)
            return CHG_DONE_FULL; /* Finish */
        
        /* Phase 2 control */
        if (seconds > CHG_TIME_FORCE)
        {
            /* Check if target voltage met */
            if (real_vbat > BATT_VOLT_TARGET)
                return CHG_DONE_FULL; /* Finish */

            /* Voltage reverse check when max voltage is higher than threshold. */
            if (real_vbat < peak - CHG_VOLT_DROP_DELTA)
                revcount ++;
            else
                revcount = 0;
            
            if (revcount > CHG_VOLT_DROP_COUNT)
                return CHG_DONE_FULL;
        
            /* Update the peak voltage, increase by 1 to avoid voltage jump. */
            if (peak < real_vbat)
                peak ++;
        }
        
        if (old_sec != seconds)
        {
            /* Temperature check every second */
            tbat = read_adc(TBAT, 3);
            
            /* Battery removal check */
            if (tbat >= TBAT_VALID_HIGH)
                return CHG_ERR_BATT_GONE;

            if (tbat - ibat * 5 / 7 <= 273) /* About 54 degree, assuming NTC is about 4Kohm around 50 degree */
                return CHG_DONE_FULL;

            old_sec = seconds;
            wdt_reset();
        }

#define TICK_PER_SECOND   262  /* Should be adjusted to match the real time */

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
    LED_ON(GREEN);
    LED_OFF(RED);
    _delay_ms(1000);

    while (1)
    {
        /* Battery removal detection */
        if (read_adc(TBAT, 3) >= TBAT_VALID_HIGH)
            return;
        
        /* Current exists means MOS out of control */
        if (read_adc(IBAT, 2) > CHG_CURRENT_THRESHOLD)
        {
            batt_alert(1);
            return;
        }

        _delay_ms(100);
        wdt_reset();
    }
}

void batt_alert(byte type)
{
    output_pwm(0);

    while (1)
    {
        /* Battery removal detection */
        if (read_adc(TBAT, 3) >= TBAT_VALID_HIGH)
        return;
        
        LED_OFF(GREEN);
        LED_ON(RED);
        _delay_ms(100);

        LED_OFF(RED);
        if (type)
            LED_ON(GREEN);
        _delay_ms(100);
        
        wdt_reset();
    }
}
