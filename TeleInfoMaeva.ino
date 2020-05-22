/**@file Teleinfo_Maeva_2_00.ino

              Datalogger Téléinfo 2 compteurs sur Arduino

 Compteur 1 : consommation en tarif HCHP triphasé
 Compteur 2 : production solaire en tarif BASE

 Affectation des ports utilisés sur Arduino UNO :
 *=======*======================================*
 * Ports * Affectation                          *
 *  D0   * Entrée des données TéléInfo          *
 *  D4   * /CS Chipselect - Validation carte SD *
 *  D5   * Validation lecture Compteur 1        *
 *  D6   * Validation lecture compteur 2        *
 *  D9   * Témoin d'écriture sur carte SD       *
 *  D10  *                                      *
 *  D11  * MOSI - Lecteur SD                    *
 *  D12  * MISO - Lecteur SD                    *
 *  D13  * SCK - Lecteur SD                     *
 *                                              *
 *  A4   * SDA - RTC DS1307                     *
 *  A5   * SCL - RTC ds1307                     *
 *=======*======================================*

 */

/**________________________________________________________________________________________________________________________
                VERSIONS HISTORY                                                        | Program space | Dynamic memory |
                                                                                        |  used / total | used  / total  |
========================================================================================|---------------|----------------|
	Version 2.00	11/04/2020	+ Original version                                          | 23474 /28672  | 790 used /8192 |
=========================================================================================================================*/
/**
 * Description des trames :
 *  Trame EDF Consomation - Heures pleines et heures creuses triphasé : groupes utilisés
 *  HCHC 014460852 $
 *  HCHP 012506372 -
 *  IINST1 002 Y
 *  IINST2 002 Y
 *  IINST3 002 Y
 *  PAPP 00520 (
 *
 *  Trame Production photovoltaique :
 *  BASE 014460852 $
 *  IINST 035 G
 *  PAPP 00520 (
=====================================================================================*/
/** __TODO__ list
 *
*/

#include "FctCommunes.h"
#include "ConsoTeleInfo.h"

#define CHIPSELECT 4

Compteur * compteur;

void setup()
{
/**
 * Initialisation des pins Compteurs 
 */
  pinMode(CPT_EDF, OUTPUT); // Compteur 1 - Conso
  pinMode(CPT_PV, OUTPUT); // Compteur 2 - Prod
  digitalWrite(CPT_EDF, HIGH);
  digitalWrite(CPT_PV, LOW);

/**
 * initialisation carte SD - pin 9
 */
pinMode(10,OUTPUT);      // obligatoire pour lib SD
pinMode(CHIPSELECT, OUTPUT);     // selection de la carte
pinMode(LED_BLEUE, OUTPUT);        // Commande de la LED témoin écriture carte SD  pinMode(ecritSD, OUTPUT);

  // Initialisation du port 0-1 lecture Téléinfo -----------------------------------------
  Serial.begin(1200,SERIAL_7E1);

  Serial.print(F("TeleInfoMaeva_V2.00\n"));

if (! SD.begin(CHIPSELECT)) {
    Serial.print(F("SD error\n"));
}

/**
 *   Initialisation de l'horloge RTC
 *   horloge inutile si maj directe de la base : horodatage fait par MySQL
 */
 if (! RTC.begin()) {
#ifdef __DEBUG
   msgLog(F("Error RTC\n"));
   Serial.println(F("Error RTC\n"));
#endif
    while (1);
  };
  if (!RTC.isrunning())
  {
#ifdef __DEBUG
    msgLog(F("Setting RTC\n"));
    Serial.print(F("Setting RTC\n"));
#endif
    RTC.adjust(DateTime(F(__DATE__),F(__TIME__)));
  };
}

void loop()
{
  ConsoTeleInfo * consoTeleInfo;
  consoTeleInfo = new ConsoTeleInfo;

  if(consoTeleInfo->readTeleInfo())
  {

    // If the frame was correctly reveived, we display it in
    // the console (only if debug)
#ifdef __DEBUG
    consoTeleInfo->displayTeleInfo(); // désactiver en mode normal
#else
    // and we record it on server
    if (!consoTeleInfo->recordTeleInfoOnMySQLServer(compteur->numero()))
    {
      // network unreached - sending frame to SD card
      consoTeleInfo->sauveSD(compteur->numero());    
    }
#endif    
  }
#ifdef __DEBUG
  else {
    Serial.print(F("No TeleInfo Data\n"));
  }
#endif
  
  delete consoTeleInfo;
  consoTeleInfo = NULL;
  compteur->bascule();
  delay(30000);
}
