#ifndef B944A19B_5CF1_40F8_A95D_CF2AA3B980F8
#define B944A19B_5CF1_40F8_A95D_CF2AA3B980F8

#include <Arduino.h>
#include <esp_system.h>

hw_timer_t *timer = NULL;

void ARDUINO_ISR_ATTR check_gps_data()
{
    //ets_printf("ISR TRIGGERED\n");
}

// Prepare hardware timer with interrupt
void setup_timer(void (*func)(void) = check_gps_data, int time_ms = 250, int div = 80)
{
    timer = timerBegin(0, div, true);              // timer 0, div 80
    timerAttachInterrupt(timer, func, true);       // attach callback
    timerAlarmWrite(timer, time_ms * 1000, false); // set time in us
    timerSetAutoReload(timer, true);
    timerAlarmEnable(timer);
}

#endif /* B944A19B_5CF1_40F8_A95D_CF2AA3B980F8 */
