/*
 * R19C - Renault 19 Computer
 * 
 * https://github.com/rohanrhu/r19c
 * 
 * Copyright (C) 2018, Oğuzhan Eroğlu (http://oguzhaneroglu.com) <rohanrhu2@gmail.com>
 * Licensed under MIT
 */ 

#define F_CPU 20000000

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>

#include "lib/5110LCD/nokia5110.h"

#define BAUD 9600
#define UBBR F_CPU/16/BAUD-1
#define T0T (1/(F_CPU/1))*1000000

void serialInit(unsigned int ubrr);
void serialSend(char data);
void serialPrint(char* str);
void timerInit(void);
uint64_t getMicros(void);
uint64_t getMillis(void);

volatile uint64_t tofwcnt = 0;
volatile uint64_t tofwcntbit = 0;

uint8_t seconds_counter = 0;
uint8_t minutes_counter = 0;
uint8_t hours_counter = 0;
uint8_t day_counter = 0;

ISR(TIMER0_OVF_vect)
{
    tofwcnt++;
    tofwcntbit = tofwcnt*256;
}

void initADC()
{
    ADMUX |= (1<<REFS0);
    ADCSRA |= (1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0)|(1<<ADEN);
}

uint16_t readADC(uint8_t adc_channel)
{
    ADMUX = (ADMUX & 0xF0) | (adc_channel & 0x0F);
    ADCSRA |= (1<<ADSC);
    while (ADCSRA & (1<<ADSC));
    return ADC;
}

#define CLOCK_MODE_NORMAL 1
#define CLOCK_MODE_SETTING_HOURS 2
#define CLOCK_MODE_SETTING_MINUTES 3
#define SETTING_MODE_QUIT_COUNTER_TIMEOUT 5

uint8_t clock_mode = CLOCK_MODE_NORMAL;
uint8_t setting_mode_quit_counter = 0;
uint8_t is_setting_button_pressed = 0;
uint8_t is_up_button_pressed = 0;
uint8_t is_down_button_pressed = 0;

uint32_t ustt0 = 0;
uint64_t ustt1 = 0;
uint32_t ustt2 = 0;

#define MAIN_MODE_CLOCK 1
#define MAIN_MODE_PARKING 2

uint8_t main_mode = MAIN_MODE_CLOCK;

int main(void)
{
    DDRD = DDRD & ~(1<<7);
    DDRB = DDRB & ~(1<<0);
    DDRC = DDRC & ~(1<<5);

    PORTB = PORTB | (1<<0);

    initADC();
    serialInit(UBBR);
    timerInit();

    uint32_t t0 = 0;
    uint32_t t1 = 0;
    uint32_t t1_1 = 0;

    nokia_lcd_init();
    
    char current_seconds[3] = {'\0'};
    char current_minutes[3] = {'\0'};
    char current_hours[3] = {'\0'};
    char time_message[6] = {'\0'};

    unsigned int seconds_number = 0;

    char value_buff[32];
    
    uint64_t ustet = getMicros();
    uint64_t ustept = getMicros();
    unsigned int measure = 0;

    while (1)
    {
        if (getMillis() - t0 >= 1000)
        {
            t0 = getMillis();
            serialPrint("SECOND!\n");
            
            if (clock_mode == CLOCK_MODE_NORMAL)
            {
                seconds_counter++;
            }

            if (seconds_counter >= 60) {
                seconds_counter = 0;
                minutes_counter++;
            }

            if (minutes_counter >= 60) {
                minutes_counter = 0;
                hours_counter++;
            }

            if (hours_counter >= 24) {
                hours_counter = 0;
            }

            setting_mode_quit_counter++;
        }
        
        if (PIND & (1<<7)) {
            main_mode = MAIN_MODE_PARKING;
        } else {
            main_mode = MAIN_MODE_CLOCK;
        }
     
        if (main_mode == MAIN_MODE_PARKING)
        {
            PORTB = PORTB | (1<<0);
            
            ustt1 = getMicros();
            while (1)
            {
                if (getMicros() - ustt1 > 10) {
                    PORTB = PORTB & ~(1<<0);
                    
                    ustet = getMicros();
                    while (1) {
                        if (PIND & (1<<6))
                        {
                            ustept = getMicros();
                            while (1) {
                                if (!(PIND & (1<<6))) {
                                    measure = ((getMicros() - ustept)/29.1)/2;
                                    nokia_lcd_clear();
                                    nokia_lcd_set_cursor(4, 0);
                                    nokia_lcd_write_string("PARK MESAFESI", 1);
                                    nokia_lcd_set_cursor(18, 15);
                                    nokia_lcd_write_string(utoa(measure, value_buff, 10), 2);
                                    nokia_lcd_set_cursor(62, 19);
                                    nokia_lcd_write_string("cm", 1);
                                    nokia_lcd_render();
                                    
                                    ustt1 = getMicros();

                                    break;
                                }
                            }

                            break;
                        } else if (getMicros() - ustet > 1000000) {
                            break;
                        }
                    }
                    break;
                }
            }
            
            ustt0 = getMillis();
            while (1) {
                if (getMillis() - ustt0 > 250) {
                    break;
                }
            }
            
            ustt1 = getMicros();
        } else {
            itoa(seconds_counter, current_seconds, 10);
            itoa(minutes_counter, current_minutes, 10);
            itoa(hours_counter, current_hours, 10);

            if (seconds_counter < 10) {
                current_seconds[1] = current_seconds[0];
                current_seconds[0] = '0';
            }

            if (minutes_counter < 10) {
                current_minutes[1] = current_minutes[0];
                current_minutes[0] = '0';
            }

            if (hours_counter < 10) {
                current_hours[1] = current_hours[0];
                current_hours[0] = '0';
            }

            time_message[0] = current_hours[0];
            time_message[1] = current_hours[1];
            time_message[2] = ':';
            time_message[3] = current_minutes[0];
            time_message[4] = current_minutes[1];
            time_message[5] = '\0';

            // -------------------------------
            
            nokia_lcd_clear();
            nokia_lcd_set_cursor(14, 0);
            nokia_lcd_write_string("RENAULT 19", 1);
            nokia_lcd_set_cursor(10, 15);
            nokia_lcd_write_string(time_message, 2);
            nokia_lcd_set_cursor(70, 19);
            nokia_lcd_write_string(current_seconds, 1);

            if (clock_mode == CLOCK_MODE_SETTING_HOURS)
            {
                if (getMillis() - t1 >= 250)
                {
                    nokia_lcd_set_cursor(10, 28);
                    nokia_lcd_write_string("--", 2);
                    
                    if (getMillis() - t1_1 > 500)
                    {
                        t1 = getMillis();
                        t1_1 = t1;
                    }
                }
            }
            else if (clock_mode == CLOCK_MODE_SETTING_MINUTES)
            {
                if (getMillis() - t1 >= 250)
                {
                    nokia_lcd_set_cursor(43, 28);
                    nokia_lcd_write_string("--", 2);
                    
                    if (getMillis() - t1_1 > 500)
                    {
                        t1 = getMillis();
                        t1_1 = t1;
                    }
                }
            }

            nokia_lcd_render();

            int button_analog = readADC(5);

            if ((is_setting_button_pressed == 0) && (button_analog > 340) && (button_analog < 450))
            {
                is_setting_button_pressed = 1;

                if (clock_mode == CLOCK_MODE_NORMAL)
                {
                    clock_mode = CLOCK_MODE_SETTING_HOURS;
                    setting_mode_quit_counter = 0;
                }
                else if (clock_mode == CLOCK_MODE_SETTING_HOURS)
                {
                    clock_mode = CLOCK_MODE_SETTING_MINUTES;
                    setting_mode_quit_counter = 0;
                }
                else if (clock_mode == CLOCK_MODE_SETTING_MINUTES)
                {
                    clock_mode = CLOCK_MODE_NORMAL;
                    setting_mode_quit_counter = 0;
                }
            }
            else if (!(button_analog > 340) || !(button_analog < 450))
            {
                is_setting_button_pressed = 0;
            }

            if (clock_mode == CLOCK_MODE_SETTING_HOURS)
            {
                
            }

            if ((clock_mode != CLOCK_MODE_NORMAL) && (setting_mode_quit_counter > SETTING_MODE_QUIT_COUNTER_TIMEOUT))
            {
                clock_mode = CLOCK_MODE_NORMAL;
                setting_mode_quit_counter = 0;
            }

            // 240 - 280
            // 150 - 200

            if ((is_up_button_pressed == 0) && (button_analog > 210) && (button_analog < 280))
            {
                is_up_button_pressed = 1;
                setting_mode_quit_counter = 0;

                if (clock_mode == CLOCK_MODE_NORMAL)
                {
                }
                else if (clock_mode == CLOCK_MODE_SETTING_HOURS)
                {
                    hours_counter++;
                }
                else if (clock_mode == CLOCK_MODE_SETTING_MINUTES)
                {
                    minutes_counter++;
                }
            }
            else if (!(button_analog > 210) || !(button_analog < 280))
            {
                is_up_button_pressed = 0;
            }

            if ((is_down_button_pressed == 0) && (button_analog > 130) && (button_analog < 200))
            {
                is_down_button_pressed = 1;
                setting_mode_quit_counter = 0;

                if (clock_mode == CLOCK_MODE_NORMAL)
                {
                }
                else if (clock_mode == CLOCK_MODE_SETTING_HOURS)
                {
                    hours_counter--;
                }
                else if (clock_mode == CLOCK_MODE_SETTING_MINUTES)
                {
                    minutes_counter--;
                }
            }
            else if (!(button_analog > 130) || !(button_analog < 200))
            {
                is_down_button_pressed = 0;
            }
        }
    }
}

void timerInit(void)
{
    cli();
    TCCR0A = 0;
    TCCR0B = 0;
    TIMSK0 |= (1 << TOIE0);
    TCCR0B |= (1 << CS00);
    sei();
}

uint64_t getMillis(void)
{
    return getMicros()/1000;
}

uint64_t getMicros(void)
{
    return ((tofwcntbit + (uint8_t)TCNT0 + 1)*0.05);
}

void serialPrint(char* str)
{
    unsigned int i = 0;

    while (str[i] != '\0')
    {
        serialSend(str[i]);
        i++;
    }
}

void serialInit(unsigned int ubrr)
{
    UBRR0H = (unsigned char)(ubrr>>8);
    UBRR0L = (unsigned char)ubrr;
    UCSR0B = (1<<TXEN0);
    UCSR0C = (1<<USBS0)|(3<<UCSZ00);
}

void serialSend(char data)
{
    while ( !( UCSR0A & (1<<UDRE0)) );
    UDR0 = data;
}