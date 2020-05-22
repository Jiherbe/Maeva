#include "FctCommunes.h"


/**
 * Constructeur de l'objet Compteur
 */
 Compteur::Compteur()
 {
  cpt = 1;
  return;
 }
/** ----------------------------------------------------------------------------------------
*Bascule la lecture entre les deux compteurs
*   ---------------------------------------------------------------------------------------*/
void Compteur::bascule()
{
    #ifdef __DEBUG
   Serial.print(F("Bacule compteur\n"));
   #endif
  if (cpt == 1)
  {
    digitalWrite(CPT_EDF, LOW);
    digitalWrite(CPT_PV, HIGH);
    cpt = 2;
  }
  else
  {
    digitalWrite(CPT_PV, LOW);
    digitalWrite(CPT_EDF, HIGH);
    cpt = 1;
  }
}                      // End of Function bascule_compteur()

/**
 * Renvoie le numéro du compteur actif.
 */
byte Compteur::numero()
{
  return cpt;
}

//=================================================================================================================
// Calculates teleinfo Checksum
//=================================================================================================================
char chksum(char *buff, uint8_t len)
{
    int i = 1;
  char sum = 0;
  while (i < len) {
      sum = sum + buff[i];
      i++;
  }
  sum = (sum & 0x3F) + 0x20;
  return(sum);
}

/**----------------------
 * Affichage des messages d'erreurs
 */
void msgLog(String msg)
{
  File mymsg;
  DateTime now = RTC.now();

  mymsg = SD.open("messages.log", FILE_WRITE);
  mymsg.print(now.unixtime());
  mymsg.print(F(" - "));
  mymsg.print(msg);
  mymsg.close();
}
