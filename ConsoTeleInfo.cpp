/** =================================================================================================================
* The ConsoTeleInfo is a class that stores the data retrieved from the teleinfo frames, and records them on the 
* mysql database Maeva. Whithout network connection, data are stored on a SD card.
* A function can displays them on the serial monitor (for debug purpose only).
* =================================================================================================================*/
#include "ConsoTeleInfo.h"

/**=================================================================================================================
* ConsoTeleInfo : Basic constructor
* =================================================================================================================*/
ConsoTeleInfo::ConsoTeleInfo()
{
  Serial.begin(1200,SERIAL_7E1);

   // variables initializations
  HCHC = 0L;  // compteur Heures Creuses en W
  HCHP = 0L;  // compteur Heures Pleines en W
  IINST1 = 0;        // intensité instantanée en A
  IINST2 = 0;        // intensité instantanée en A
  IINST3 = 0;        // intensité instantanée en A
  PAPP = 0;         // puissance apparente en VA
  return;
}

//=================================================================================================================
// Capture des trames de Teleinfo
//=================================================================================================================
boolean ConsoTeleInfo::readTeleInfo()
{
#define debTrame 0x02           // caractère de début de trame
#define debGroupe 0x0A          // caractère de début de groupe
#define finGroupe 0x0D          // caractère de fin de groupe
#define endTrame 0x03           // caractère de fin de trame
#define maxFrameLen 280

  int comptChar=0; // variable de comptage des caractères reçus
  char inByte=0; // variable de mémorisation du caractère courant en réception
  char buffTeleinfo[21] = "";
  int bufflen = 0;
  int nbg= 0;    // number of information group
  boolean isOK = false;

  if (Serial.available())
  {
    while (inByte != debTrame) //--- waiting for starting frame character
    {
      if (Serial.available())
        inByte = Serial.read(); // Serial.read() vide buffer au fur et à mesure
    }
    while (inByte != endTrame)
    {
      if (Serial.available() ) {
        inByte = Serial.read();
        comptChar++;
        if (inByte == debGroupe)
        {
          bufflen = 0;
        }
        buffTeleinfo[bufflen] = inByte;
        bufflen++;
  
        if (inByte == finGroupe)
        {
          if (chksum(buffTeleinfo,bufflen-2) == buffTeleinfo[bufflen-2]) // Test du Checksum
          {
            nbg += decodeTrame(buffTeleinfo);
            if ((nbg ==3) || (nbg == 6)) {
              Serial.flush();
              buffTeleinfo[0]='\0';
              isOK = true;
            }
          }               // End if checkSum
        }                 // End if (inByte == finGroupe)
      }                   // End if (Serial.available() )
      if (comptChar > maxFrameLen)
      {
        Serial.println(F("Overflow error ..."));
        isOK = false;
      }
    }
  } else
  {
    Serial.print(F("No data input\n"));
    isOK = false;
  }
  return isOK;
}


/**--------------------------------------------------------------------------------
 * Analyse des groupes trouvés
 * 
 *--------------------------------------------------------------------------------*/
int ConsoTeleInfo::decodeTrame(char *buff)
{
  int grpTrouve = 0;

  if ((strncmp("HCHC ", &buff[1] , 5)==0) || (strncmp("BASE ", &buff[1] , 5)==0)){
     HCHC = atol(&buff[6]);
     grpTrouve++;
  }
  else if (strncmp("HCHP ", &buff[1] , 5)==0){
     HCHP = atol(&buff[6]);
     grpTrouve++;
  }
  else if ((strncmp("IINST1 ", &buff[1] , 7)==0) || (strncmp("IINST ", &buff[1] , 6)==0)){
     IINST1 = atol(&buff[8]);
     grpTrouve++;
  }
  else if (strncmp("IINST2 ", &buff[1] , 7)==0){
     IINST2 = atol(&buff[8]);
     grpTrouve++;
  }
  else if (strncmp("IINST3 ", &buff[1] , 7)==0){
     IINST3 = atol(&buff[8]);
     grpTrouve++;
  }
  else if (strncmp("PAPP ", &buff[1] , 5)==0){
     PAPP = atol(&buff[6]);
     grpTrouve++;
  }
  
  return grpTrouve;
}

//=================================================================================================================
// This function displays the TeleInfo Internal counters
// It's usefull for debug purpose.
// In released version, it save data on a SD card when there in no connexion.
//=================================================================================================================
#ifdef __DEBUG
void ConsoTeleInfo::displayTeleInfo()
{  /*
ADCO 270622224349 B
 OPTARIF HC.. <
 ISOUSC 30 9
 HCHC 014460852 $
 HCHP 012506372 -
 PTEC HP..
 IINST 002 Y
 IMAX 035 G
 PAPP 00520 (
 HHPHC C .
 MOTDETAT 000000 B
 */
  Serial.print(F(" "));
  Serial.println();
  Serial.print(F("HCHC "));
  Serial.println(HCHC);
  Serial.print(F("HCHP "));
  Serial.println(HCHP);
  Serial.print(F("IINST1 "));
  Serial.println(IINST1);
  Serial.print(F("IINST2 "));
  Serial.println(IINST2);
  Serial.print(F("IINST3 "));
  Serial.println(IINST3);
  Serial.print(F("PAPP "));
  Serial.println(PAPP);
}
#endif
//=================================================================================================================
// Send results to PHP server
//=================================================================================================================
boolean ConsoTeleInfo::recordTeleInfoOnMySQLServer(const byte cpt)
{
  uint8_t macAddr[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
  IPAddress serverAddr(192,168,1,14);
  IPAddress ip(192, 168, 1, 250);
  IPAddress myDns(192, 168, 1, 1);
  EthernetClient client;
  Ethernet.init(10);
  Ethernet.begin(macAddr, ip, myDns);

  if (client.connect(serverAddr, 8088)) {
#ifdef __DEBUG
    Serial.print(F("Connected to "));
    Serial.println(client.remoteIP());
#endif
    /**
     *  Make a HTTP request:
     */
    client.print(F("GET /Maeva/bin/majBase.php?cpt="));
    client.print(cpt);
    client.print(F("&ind1="));
    client.print(HCHC);
    if (cpt == 1) {
      client.print(F("&ind2="));
      client.print(HCHP);
    }
    client.print(F("&int1="));
    client.print(IINST1);
    if (cpt == 1) {
      client.print(F("&int2="));
      client.print(IINST2);
      client.print(F("&int3="));
      client.print(IINST3);
    }
    client.print(F("&pap="));
    client.print(PAPP);
    client.println(F(" HTTP/1.1"));
    client.println(F("Host: nase510ec.local"));
    client.println(F("Content-Type: application/x-www-form-urlencoded"));
    client.println(F("User-Agent : ARDUINO, TeleInfo"));
    client.println(F("Connection: close"));
    client.println();
    // receiving data back from the HTTP server
    client.stop();
#ifdef __DEBUG
    Serial.println(F("\nFrame sent ...\n"));
#endif
    return true;
  }
  else
  {
    // if you didn't get a connection to the server:
    client.flush();
#ifdef __DEBUG
    Serial.print(F("Connection failed, sending to SDcard\n"));
#else
    msgLog("Connection failed, sending to SDcard");
#endif
    return false;
  }
}

void ConsoTeleInfo::sauveSD(const byte cpt)
{
  char fichierJournee[13];
  File teleinfoFile;

//   lecture de l'horloge
  DateTime now = RTC.now();

  sprintf(fichierJournee, "DATAC%02d.CSV", cpt);

  digitalWrite(LED_BLEUE, HIGH); // allume le témoin d'écriture sur carte
  if ((teleinfoFile = SD.open(fichierJournee, FILE_WRITE)))
  {
    teleinfoFile.print(now.unixtime());
    teleinfoFile.print(",");
    switch (cpt) {
      case 1: {
        teleinfoFile.print(HCHC);
        teleinfoFile.print(",");
        teleinfoFile.print(HCHP);
        teleinfoFile.print(",");
        teleinfoFile.print(IINST1);
        teleinfoFile.print(",");
        teleinfoFile.print(IINST2);
        teleinfoFile.print(",");
        teleinfoFile.print(IINST3);
        teleinfoFile.print(",");
        teleinfoFile.println(PAPP);
        break;
      }
      case 2: {
        teleinfoFile.print(HCHC);
        teleinfoFile.print(",");
        teleinfoFile.print(IINST1);
        teleinfoFile.print(",");
        teleinfoFile.println(PAPP);
        break;
      }
    }
    teleinfoFile.flush();   //  fermeture du fichier
    teleinfoFile.close();
  }
#ifdef __DEBUG
  else      // si le fichier ne peut pas être ouvert alors erreur !
  {
    Serial.print(now.unixtime());
    Serial.print(F(" - File open error : "));
    Serial.println(fichierJournee);
  }
#endif
  digitalWrite(LED_BLEUE, LOW); // extinction du témoin
}
