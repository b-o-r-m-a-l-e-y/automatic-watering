#define __AVR_ATtiny13A__
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

//Freq CPUio 9.6 MHz/8=1.2MHz

#define WATER_PUPM_PIN 3

#define BATTERY_MASK          (0<<MUX1)|(1<<MUX0)
#define SENSOR_MASK           (1<<MUX1)|(0<<MUX0)

//Resistor divider Up = 680k and Down = 150k
#define ADC_MAX 1024
#define BATTERY_LOW_LIMIT       768 //The lowest voltage for battery = 3.4 Volts
#define SOIL_LOW_LIMIT          300 //The lower, the more dusty soil is

#define WATER_PUMP_TIME_MAX     4000 //How much time water pump os on

#define UPDATE_TIME             8/8 //In seconds. Should be divided by 8

uint8_t waterPumpFlag = 0;
uint8_t goToSleepFlag = 0;
volatile uint16_t msCounter = 0;
volatile uint16_t waterPumpTime = 0;

uint8_t volatile WDT8msCounter = 0; // Varible will be incremented in WDG timer

inline void configuration()
{
    cli();
    DDRB = (1<<PIN0)|(1<<PIN1)|(1<<PIN3);
    //Pull-up for button NEED CORRECT FUSE
    PORTB = (1<<PORTB5);

    WDTCR = WDCE<<1;
    //Watchdog interrupt each 8s
    WDTCR |= (1<<WDP3)|(0<<WDP2)|(0<<WDP1)|(1<<WDP0);
    //Enable WD interrupt
    WDTCR |= (1<<WDTIE);

    //ADC internal reference, right adjust PB2 (soil sensor)
    ADMUX = (1<<REFS0)|(0<<ADLAR)|(0<<MUX1)|(1<<MUX0);
    //Enable ADC
    ADCSRA |= (1<<ADEN);
    ADCSRA &= ~(1<<ADIF);

    //Timer for 1 ms CTC Mode
    //Enable interrupt on Output Compare Match A
    TIMSK0=(1<<OCIE0A);
    TCCR0A=(1<<WGM01)|(0<<WGM00);
    //Prescaler 256
    TCCR0B=(1<<CS02);
    TCNT0=0;
    OCR0A=47;

    set_sleep_mode(SLEEP_MODE_PWR_DOWN);

    sei();
}

uint16_t getADCResult(uint8_t mask)
{
    ADMUX ^= mask;
    //Start convertion
    ADCSRA |= (1<<ADSC);
    while (!(ADCSRA & 1<<ADIF)); //Wait util conversion complete
    ADCSRA &= ~(1<<ADIF);
    uint16_t data = 0;
    data = (ADCH<<8)| ADCL;
    return data;
}

/*
 * Checking battery voltage. If low, light up LED on PB0
 */
void batteryCheck()
{
    uint16_t batVoltage = 0;
    batVoltage = getADCResult(BATTERY_MASK);
    if (batVoltage<BATTERY_LOW_LIMIT) PORTB |= (1<<PORTB0);
    else PORTB &= (~1<<PORTB0);
}
/*
 * Check voltage on resistor divider
 */
void soilSensorCheck()
{
    PORTB |= (1<<PORTB1); //Open transistor for sensor
    uint16_t soilVoltage = 1024;
    soilVoltage = getADCResult(SENSOR_MASK);
    if (soilVoltage<SOIL_LOW_LIMIT) waterPumpFlag=1;
    PORTB &= ~(1<<PORTB1); //Close transistor
}

int main()
{
    configuration();
    while(1)
    {
        if (WDT8msCounter>UPDATE_TIME)
        {
            batteryCheck();
            soilSensorCheck();
            cli();
            WDT8msCounter=0;
            sei();
        }
        if(waterPumpFlag)
        {
            PORTB |= (1<<PORTB3); //Turn on water pump
            if(waterPumpTime>=WATER_PUMP_TIME_MAX)
            {
                PORTB &= !(1<<PORTB3);
                cli();
                waterPumpFlag=0;
                waterPumpTime=0;
                goToSleepFlag=1;
                sei();
            }
        }
        else goToSleepFlag = 1;
        if(goToSleepFlag) 
        {
            cli();
            sleep_enable();
            sei();
            sleep_cpu();
            goToSleepFlag = 0;
        }
    }
    return 0;
}

ISR(TIM0_COMPA_vect)
{
    cli();
    if(waterPumpFlag) waterPumpTime++;
    else waterPumpTime = 0;
    sei();
}

ISR(WDT_vect)
{
    WDT8msCounter++;
}