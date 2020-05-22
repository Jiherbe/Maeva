#ifndef __FCTCOMMUNES_H_INCLUDED
#define __FCTCOMMUNES_H_INCLUDED

// Librairies Arduino
#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <RTClib.h>
#include <SD.h>

#define __DEBUG
#define LED_BLEUE 9

class Compteur
{
  public:
    Compteur();
    void bascule();
    byte numero();

  private:
    #define CPT_EDF 5
    #define CPT_PV 6
    byte cpt;  
};

extern RTC_DS1307 RTC;
char chksum(char *, uint8_t);
void msgLog(String);

#endif // __FCTCOMMUNES_H_INCLUDED
