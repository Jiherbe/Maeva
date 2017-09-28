
/*
              Datalogger Téléinfo 2 compteurs sur Arduino
 
 Compteur 1: consommation
 Compteur 2: production solaire en tarif BASE
 
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
 
 Juin 2011:
 v0.2  * passage automatique à l'heure d'été
 * correctif erreur abo BBR
 * modification ecriture sur SD (utilisation de teleinfoFile.print à la place s'une variable STRING
 qui plante l'Arduino avec les abonnements BBR
 v0.2a * ajout mode sans puissance apparente pour ancien compteur et calcul de celle-ci pour les logiciels d'analyse
 v0.2b * pour Arduino 1.0 (mise à jour de la RTC et utilisation de la librairie SD livrée avec la 1.0)              
 v0.2c * modif type de variable pour éviter d'avoir 2 enregistrements à la même minute (surtout sur des proc + rapide)
 v0.3  * détection automatique du type d'abonnement sur le compteur 1
 v0.3a * Fonction de mise à l'heure par l'usb (interface série) en 1200 bauds 7 bits parité pair
         Procédure:
         1- carte débranchée, enlevez la pile de son support (pour réinitialiser l'horloge)
         2- enlevez le cavalier du shield téléinfo
         3- configurer votre logiciel émulateur de terminal (termite,Hyperterminale...) en 1200 bauds 7 bits parité pair
         4- mettre sous tension la carte.
         -- Le programme détectera le reset de l'horloge et vous demandera de rentrer l'heure et la date. --
 
 v0.3b * Correction bug calcul PAP compteur 2, enregistrement journalier compteur 2
 v1.00 * Adaptation à l’installation MAEVA
 
 Jan 2014:
 v1.10 * Remplacement shield memoire Snootlab par shield Cartelectronic : chipselect = 4
 
 Fev 2014:
 v1.11 * Modification de la structure des fichiers (Date)
         Ajout témoin d'écriture Sur carte SD
         Simplification des écritures sur carte SD
         Suppression des appels inutiles aux fonctions de mise en forme date.
 v1.12 * correction bug sur stockage mois-jour : longueur de chaine incorrecte.
 */

#include <SD.h>
#include <Wire.h>
#include <RTClib.h>

const char version_logiciel[6] = "V1.12";

//#define echo_USB              // envoie toutes les trames téléinfo sur l'USB
//#define message_systeme_USB   //envoie des messages sur l'USB (init SD, heure au demarrage, et echo des erreurs)
//#define option_Web            // Reserve pour version future
File mymsg;                   // fichier log pour debug
//boolean ecritLog = false;     // pour éviter de saturer la carte avec msg absence téléinfo

//*****************************************************************************************
byte inByte = 0 ;        // caractère entrant téléinfo
char buffteleinfo[21] = "";
byte bufflen = 0;
byte mem_sauv_minute = 1;
byte mem_sauv_journee = 1;
byte num_abo = 0;
byte type_mono_tri = 0;
uint8_t presence_teleinfo = 0;      // si signal teleinfo présent
byte presence_PAP = 0;           // si PAP présent
boolean cpt2_present = true;     // mémorisation de la présence du compteur 2
boolean compteursluOK = false;// pour autoriser l'écriture de valeur sur la SD

int ReceptionOctet = 0; // variable de stockage des octets reçus par port série
unsigned int ReceptionNombre = 0; // variable de calcul du nombre reçu par port série
byte reg_horloge = 1; 
boolean mem_reg_horloge = false; 
const uint8_t val_max[6] = { 24,59,59,31,12,99};

// declarations Teleinfo
unsigned int papp = 0;  // Puissance apparente, VA

uint8_t IINST1 = 0; // Intensité Instantanée Phase 1, A  (intensité efficace instantanée) ou 1 phase en monophasé
uint8_t IINST2 = 0; // Intensité Instantanée Phase 2, A  (intensité efficace instantanée)
uint8_t IINST3 = 0; // Intensité Instantanée Phase 3, A  (intensité efficace instantanée)

unsigned long INDEX1 = 0;    // Index option Tempo - Heures Creuses Jours Bleus, Wh
unsigned long INDEX2 = 0;    // Index option Tempo - Heures Pleines Jours Bleus, Wh
unsigned long INDEX3 = 0;    // Index option Tempo - Heures Creuses Jours Blancs, Wh
unsigned long INDEX4 = 0;    // Index option Tempo - Heures Pleines Jours Blancs, Wh
unsigned long INDEX5 = 0;    // Index option Tempo - Heures Creuses Jours Rouges, Wh
unsigned long INDEX6 = 0;    // Index option Tempo - Heures Pleines Jours Rouges, Wh

// compteur 2 (solaire configuré en tarif BASE par ERDF)
unsigned long cpt2index = 0; // Index option Base compteur production solaire, Wh
unsigned int cpt2puissance = 0;  // Puissance apparente compteur production solaire, VA
unsigned int cpt2intensite = 0; // Monophasé - Intensité Instantanée compteur production solaire, A  (intensité efficace instantanée)

#define debtrame 0x02
#define debligne 0x0A
#define finligne 0x0D

// *************** déclaration carte micro SD ******************
const byte chipSelect = 4;      // selection carte
boolean statusSD = false;
const byte ecritSD = 9;         // pour allumage LED témoin d'écriture

// *************** déclaration activation compteur 1 ou 2 ******
#define LEC_CPT1 5  // lecture compteur 1
#define LEC_CPT2 6  // lecture compteur 2
//
byte verif_cpt_lu = 0;
//

byte compteur_actif = 1;  // numero du compteur en cours de lecture
byte donnee_ok_cpt1 = 0;  // pour vérifier que les donnees sont bien en memoire avant ecriture dans fichier
byte donnee_ok_cpt2 = 0;
byte donnee_ok_cpt1_ph = 0;

// *************** variables RTC ************************************
byte minute, heure, seconde, jour, mois, jour_semaine;
unsigned int annee;
char date_heure[17];    // format : "AAAA-MM-JJ HH:MM"
char mois_jour[11];     // format : "AAAA-MM-JJ"
byte mem_chg_heure = 0; //pour pas passer perpetuellement de 3h à 2h du matin le dernier dimanche d'octobre
RTC_DS1307 RTC;

// ************** initialisation *******************************
void setup() 
{
  // initialisation du port 0-1 lecture Téléinfo
  Serial.begin(1200);
  // parité paire E
  // 7 bits data
  UCSR0C = B00100100;

  // initialisation des sorties selection compteur
  pinMode(LEC_CPT1, OUTPUT);
  pinMode(LEC_CPT2, OUTPUT);
  digitalWrite(LEC_CPT1, HIGH);
  digitalWrite(LEC_CPT2, LOW);

  // verification de la présence de la microSD et si elle est initialisée :
  pinMode(10, OUTPUT);             // Obligatoire pour lib SD
  pinMode(chipSelect, OUTPUT);     // Sélection Carte SD

  statusSD = SD.begin(chipSelect); // Vérifie le lecteur de carte : TRUE si OK
  if (! statusSD)                              
  {
#ifdef message_systeme_USB
    Serial.println(F("> Erreur carte, ou carte absente !"));
#endif
    return;
  }

  // initialisation de la LED écriture SD
  pinMode(ecritSD, OUTPUT);        // Commande de la LED témoin écriture carte SD  pinMode(ecritSD, OUTPUT);
  digitalWrite(ecritSD, LOW);      // LED éteinte par défaut
  
  // initialisation RTC
  Wire.begin();
  RTC.begin();

  if (! RTC.isrunning())
  {
    // sets the RTC to the date & time this sketch was compiled
    RTC.adjust(DateTime(__DATE__, __TIME__));
  }

  DateTime now = RTC.now();     // lecture de l'horloge
  annee = now.year();
  mois = now.month();
  jour = now.day();
  heure = now.hour();
  minute = now.minute();
  jour_semaine = now.dayOfWeek();

//  format_date_heure();

// Message au démarrage initial.  
  sprintf(date_heure,"%d-%02d-%02d %02d:%02d", annee, mois, jour, heure, minute);
  mymsg = SD.open("messages.log", FILE_WRITE);
  mymsg.print(F("Version TeleInfo : "));
  mymsg.println(version_logiciel);
  mymsg.print(date_heure);
  mymsg.println(F(" > microSD initialisee !"));
  mymsg.close();
}

// ************** boucle principale *******************************

void loop()                     // Programme en boucle
{
  if (!(RTC.isrunning()) && (reg_horloge < 7))
  {      // si l'horloge n'est pas configurée (changement pile)
    maj_Horloge();
  }  	
  else
  {
    DateTime now = RTC.now();     // lecture de l'horloge
    minute = now.minute();
    heure = now.hour();
    seconde = now.second();

    if ((heure == 0) && (minute == 0) && (seconde ==0))
    {
      annee = now.year();
      mois = now.month();
      jour = now.day();
      jour_semaine = now.dayOfWeek();
    }

    // passage à l'heure d'été +1 heure
    // la lib RTC a une fonction: dayOfWeek qui donne le jour de la semaine (la DS1307 se charge de tout !)
    // réponse: 0 -> dimanche, 1 -> lundi etc...
    //
    if ((heure == 2) && (minute == 0) && (seconde == 0) && (jour_semaine == 0) && (mois == 3) && (jour > 24))
    {
      heure = 3;
      RTCsetTime();
    }

    // passage à l'heure d'hiver -1 heure
    if ((heure == 3) && (minute == 0) && (seconde == 0) && (jour_semaine == 0) && (mois == 10) && (jour > 24) && (mem_chg_heure == 0))
    {
      heure = 2;
      RTCsetTime();
      mem_chg_heure = 1;
    }
    
//============= Ajout du 3/06 ===    V0.3c    
// un seul enregistrement par minute !  
  if ((seconde == 1) && (mem_sauv_minute == 0)) 
  {
     digitalWrite(ecritSD, HIGH); // Témoin d'écriture carte  
     mem_sauv_minute = 1;
//     format_date_heure();
     enregistre(); 
     compteursluOK = false;
     digitalWrite(ecritSD, LOW);  
  }
// seconde = 10 pour être sur de pas tomber pendant l'enregistrement toutes les minutes  
  if ((heure == 23) && (minute == 59) && (seconde == 10) && (mem_sauv_journee == 0)) 
  {
     digitalWrite(ecritSD, HIGH); // Témoin d'écriture carte
     mem_sauv_journee = 1;
//     format_mois_jour();
     fichier_annee();
     compteursluOK = false;
     digitalWrite(ecritSD, LOW);  
  }
// Pour éviter doublons enregistrement
  if (seconde > 10)                       
  {
     mem_sauv_journee = 0;
     mem_sauv_minute = 0;
  }
//==================================================
  if ((donnee_ok_cpt1 == verif_cpt_lu) && (cpt2_present) && (donnee_ok_cpt1_ph == B10000111))
  {
#ifdef message_systeme_USB
      mymsg = SD.open("messages.log", FILE_WRITE);
      mymsg.println("CPT1 OK _ Appel bascule compteurs");
      mymsg.close();
#endif
      bascule_compteur();
    }
    else if ((donnee_ok_cpt2 == B00000111) or ((compteur_actif == 2) and (!cpt2_present)))
    {
#ifdef message_systeme_USB
      mymsg = SD.open("messages.log", FILE_WRITE);
      mymsg.println("CPT2 OK _ Appel bascule compteurs");
      mymsg.close();
#endif      
      bascule_compteur();
      compteursluOK = true;
    }
    if (compteur_actif == 2)
    {
      if (presence_teleinfo>200) cpt2_present = false;
      else cpt2_present = true;
    }

    read_teleinfo();
  }
}

///////////////////////////////////////////////////////////////////
// Calcul Checksum teleinfo
///////////////////////////////////////////////////////////////////
char chksum(char *buff, uint8_t len)
{
  int i;
  char sum = 0;
  for (i=1; i<(len-2); i++) sum = sum + buff[i];
  sum = (sum & 0x3F) + 0x20;
  return(sum);
}

///////////////////////////////////////////////////////////////////
// Analyse de la ligne de Teleinfo
///////////////////////////////////////////////////////////////////
void traitbuf_cpt(char *buff, uint8_t len)
{
  char optarif[4]= "";    // Option tarifaire choisie: BASE => Option Base, HC.. => Option Heures Creuses, 
  // EJP. => Option EJP, BBRx => Option Tempo [x selon contacts auxiliaires]

  if (compteur_actif == 1){

    if (num_abo == 0){ // détermine le type d'abonnement

      if (strncmp("OPTARIF ", &buff[1] , 8)==0){
        strncpy(optarif, &buff[9], 3);
        optarif[3]='\0';

        if (strcmp("BAS", optarif)==0) {
          num_abo = 1;
          verif_cpt_lu = B00000001;
        }
        else if (strcmp("HC.", optarif)==0) {
          num_abo = 2;
          verif_cpt_lu = B00000011;
        }
        else if (strcmp("EJP", optarif)==0) {
          num_abo = 3;
          verif_cpt_lu = B00000011;
        }
        else if (strcmp("BBR", optarif)==0) {
          num_abo = 4;
          verif_cpt_lu = B00111111;
        }
      }
    }
    else {

      if (num_abo == 1){
        if (strncmp("BASE ", &buff[1] , 5)==0){
          INDEX1 = atol(&buff[6]);
          donnee_ok_cpt1 = donnee_ok_cpt1 | B00000001;
        }
      }

      else if (num_abo == 2){
        if (strncmp("HCHP ", &buff[1] , 5)==0){
          INDEX1 = atol(&buff[6]);
          donnee_ok_cpt1 = donnee_ok_cpt1 | B00000001;
        }
        else if (strncmp("HCHC ", &buff[1] , 5)==0){
          INDEX2 = atol(&buff[6]);
          donnee_ok_cpt1 = donnee_ok_cpt1 | B00000010;
        }
      }

      else if (num_abo == 3){
        if (strncmp("EJPHN ", &buff[1] , 6)==0){
          INDEX1 = atol(&buff[7]);
          donnee_ok_cpt1 = donnee_ok_cpt1 | B00000001;
        }
        else if (strncmp("EJPHPM ", &buff[1] , 7)==0){
          INDEX2 = atol(&buff[8]);
          donnee_ok_cpt1 = donnee_ok_cpt1 | B00000010;
        }
      }

      else if (num_abo == 4){
        if (strncmp("BBRHCJB ", &buff[1] , 8)==0){
          INDEX1 = atol(&buff[9]);
          donnee_ok_cpt1 = donnee_ok_cpt1 | B00000001;
        }
        else if (strncmp("BBRHPJB ", &buff[1] , 8)==0){
          INDEX2 = atol(&buff[9]);
          donnee_ok_cpt1 = donnee_ok_cpt1 | B00000010;
        }
        else if (strncmp("BBRHCJW ", &buff[1] , 8)==0){
          INDEX3 = atol(&buff[9]);
          donnee_ok_cpt1 = donnee_ok_cpt1 | B00000100;
        }
        else if (strncmp("BBRHPJW ", &buff[1] , 8)==0){
          INDEX4 = atol(&buff[9]);
          donnee_ok_cpt1 = donnee_ok_cpt1 | B00001000;
        }
        else if (strncmp("BBRHCJR ", &buff[1] , 8)==0){
          INDEX5 = atol(&buff[9]);
          donnee_ok_cpt1 = donnee_ok_cpt1 | B00010000;
        }
        else if (strncmp("BBRHPJR ", &buff[1] , 8)==0){
          INDEX6 = atol(&buff[9]);
          donnee_ok_cpt1 = donnee_ok_cpt1 | B00100000;
        }
      }
    }

    if (type_mono_tri == 0)
    { // détermine le type mono ou triphasé

      if (strncmp("IINST", &buff[1] , 5)==0) {
        if (strncmp("IINST ", &buff[1] , 6)==0) type_mono_tri  = 1; // monophasé
        else if (strncmp("IINST1 ", &buff[1] , 7)==0) type_mono_tri = 3; // triphasé
        else if (strncmp("IINST2 ", &buff[1] , 7)==0) type_mono_tri = 3; // triphasé
        else if (strncmp("IINST3 ", &buff[1] , 7)==0) type_mono_tri = 3; // triphasé
      }
    }
    else
    {
      if (type_mono_tri == 1) {
        if (strncmp("IINST ", &buff[1] , 6)==0){ 
          IINST1 = atol(&buff[7]);
          donnee_ok_cpt1_ph = donnee_ok_cpt1_ph | B00000001;
          presence_PAP++;
        }
      }
      else if (type_mono_tri == 3) {
        if (strncmp("IINST1 ", &buff[1] , 7)==0){ 
          IINST1 = atol(&buff[8]);
          donnee_ok_cpt1_ph = donnee_ok_cpt1_ph | B00000001;
        }
        else if (strncmp("IINST2 ", &buff[1] , 7)==0){ 
          IINST2 = atol(&buff[8]);
          donnee_ok_cpt1_ph = donnee_ok_cpt1_ph | B00000010;
        }
        else if (strncmp("IINST3 ", &buff[1] , 7)==0){ 
          IINST3 = atol(&buff[8]);
          donnee_ok_cpt1_ph = donnee_ok_cpt1_ph | B00000100;
          presence_PAP++;
        }
      }
    }

    if (strncmp("PAPP ", &buff[1] , 5)==0){
      papp = atol(&buff[6]);
      donnee_ok_cpt1_ph = donnee_ok_cpt1_ph | B10000000;
    }
    // si pas d'index puissance apparente (PAP) calcul de la puissance
    if ((presence_PAP > 2) && !(donnee_ok_cpt1_ph & B10000000)) {
      if (type_mono_tri == 1) papp = IINST1 * 240;
      if (type_mono_tri == 3) papp = IINST1 * 240 + IINST2 * 240 + IINST3 * 240;
      donnee_ok_cpt1_ph = donnee_ok_cpt1_ph | B10000000;
    }

  }
  if (compteur_actif == 2){
    if (strncmp("BASE ", &buff[1] , 5)==0){
      cpt2index = atol(&buff[6]);
      donnee_ok_cpt2 = donnee_ok_cpt2 | B00000001;
    }
    else if (strncmp("IINST ", &buff[1] , 6)==0){ 
      cpt2intensite = atol(&buff[7]);
      donnee_ok_cpt2 = donnee_ok_cpt2 | B00000010;
      presence_PAP++;
    }
    else if (strncmp("PAPP ", &buff[1] , 5)==0){
      cpt2puissance = atol(&buff[6]);
      donnee_ok_cpt2 = donnee_ok_cpt2 | B00000100;
    }
    // si pas d'index puissance apparente (PAP) calcul de la puissance
    if ((presence_PAP > 2) && !(donnee_ok_cpt2 & B00000100)) {
      cpt2puissance = cpt2intensite * 240;
      donnee_ok_cpt2 = donnee_ok_cpt2 | B00000100;
    }
  }
}

///////////////////////////////////////////////////////////////////
// Changement de lecture de compteur
///////////////////////////////////////////////////////////////////
void bascule_compteur()
{
  if (compteur_actif == 1)
  {
    digitalWrite(LEC_CPT1, LOW);
    digitalWrite(LEC_CPT2, HIGH);
    compteur_actif = 2;
  }
  else
  {
    digitalWrite(LEC_CPT2, LOW);
    digitalWrite(LEC_CPT1, HIGH);
    compteur_actif = 1;
  }

  donnee_ok_cpt1 = B00000000;
  donnee_ok_cpt2 = B00000000;
  donnee_ok_cpt1_ph = B00000000;
  presence_PAP = 0;
  presence_teleinfo = 0;
  bufflen=0;

  Serial.flush(); 
  buffteleinfo[0]='\0';
  delay(500);
}

/******************************************************************
*  Lecture trame teleinfo (ligne par ligne)
******************************************************************/ 
void read_teleinfo()
{
  presence_teleinfo++;
  if (presence_teleinfo > 250) presence_teleinfo = 0;
  // si une donnée est dispo sur le port série
  if (Serial.available() > 0) 
  {
     presence_teleinfo = 0;
    // recupère le caractère dispo
    inByte = Serial.read();

    if (inByte == debtrame) bufflen = 0; // test le début de trame
    if (inByte == debligne) // test si c'est le caractère de début de ligne
    {
      bufflen = 0;
    }  
    buffteleinfo[bufflen] = inByte;
    bufflen++;
    if (bufflen > 21)bufflen=0;
    if (inByte == finligne && bufflen > 5) // si Fin de ligne trouvée 
    {
      if (chksum(buffteleinfo,bufflen-1)== buffteleinfo[bufflen-2]) // Test du Checksum
      {
        traitbuf_cpt(buffteleinfo,bufflen-1); // ChekSum OK => Analyse de la Trame
      }
    }
  }
/*
  else
  {
    mymsg = SD.open("messages.log", FILE_WRITE);
    mymsg.print(date_heure);
    mymsg.print(F(" - Pas de données Téléinfo compteur "));
    mymsg.println(compteur_actif);
    mymsg.close();
  }
*/
}

/*
*-----------------------------------------------------------------
*   Enregistrement trame Teleinfo toutes les minutes.
*   Si le fichier n'existe pas (nouvelle journée), il est créé.
*   L'entete du fichier n'est utile que pour un traitement par tableur.
*   Dans le cas d'une base de données, il devra etre supprimé
*-----------------------------------------------------------------
*/
void enregistre()
{
  char fichier_journee[13];
  File teleinfoFile;

 // Mise en forme de la date au format DATETIME de MySQL
  sprintf(date_heure,"%d-%02d-%02d %02d:%02d", annee, mois, jour, heure, minute);

  sprintf(fichier_journee, "TI-%02d-%02d.csv", mois,jour ); // nom du fichier court (8 caractères maxi . 3 caractères maxi)
  if (!SD.exists(fichier_journee))                           // si le fichier n'existe pas encore    
  {
    teleinfoFile = SD.open(fichier_journee, FILE_WRITE);     // il est créé et on écrit l'en-tete
#ifndef option_Web    
    teleinfoFile.println(F("Date hh:mm,Heures Creuses,Heures Pleines,I ph1,I ph2,I ph3,P VA,Index Solaire,I A Solaire,P VA Solaire"));
#endif
  }
  else teleinfoFile = SD.open(fichier_journee, FILE_WRITE);  // le fichier existe et on l'ouvre en écriture à partir de la fin
 
  if (teleinfoFile)                                          // le fichier est bien ouvert-> écriture
  {
    teleinfoFile.print(date_heure);
    teleinfoFile.print(",");
    teleinfoFile.print(INDEX1);
    teleinfoFile.print(",");
    teleinfoFile.print(INDEX2);
    teleinfoFile.print(",");
    teleinfoFile.print(IINST1);
    teleinfoFile.print(",");
    teleinfoFile.print(IINST2);
    teleinfoFile.print(",");
    teleinfoFile.print(IINST3);
    teleinfoFile.print(",");
    teleinfoFile.print(papp);
    teleinfoFile.print(",");
    teleinfoFile.print(cpt2index);
    teleinfoFile.print(",");
    teleinfoFile.print(cpt2intensite);
    teleinfoFile.print(",");
    teleinfoFile.println(cpt2puissance);
    
    teleinfoFile.flush();                                    // fermeture du fichier
    teleinfoFile.close();
  }
  else                                                       // si le fichier ne peut pas être ouvert alors erreur !
  {
    mymsg = SD.open("messages.log", FILE_WRITE);
    mymsg.print(date_heure);
    mymsg.println(F(" - Erreur ouverture fichier journee"));
    mymsg.close();
  } 
}                                                            // End of Function enregistre()

/*
*-------------------------------------------------------------------------
*   Enregistrement de l'index journalier teleinfo dans fichier annuel.
*   Si le fichier n'existe pas, il est créé.
*   L'entete du fichier n'est utile que pour un traitement par tableur.
*   Dans le cas d'une base de données, il devra etre supprimé
*-------------------------------------------------------------------------
*/
void fichier_annee()
{
   char fichier_annee[13];
   File annetelefile;
   
  // date au format DATETIME de MySQL
   sprintf(mois_jour,"%d-%02d-%02d,", annee, mois, jour);
  
   sprintf(fichier_annee, "TELE%d.csv", annee ); // nom du fichier court (8 caractères maxi . 3 caractères maxi)
   if (!SD.exists(fichier_annee))                // si le fichier n'existe pas encore
   { 
      annetelefile = SD.open(fichier_annee, FILE_WRITE); // il est créé et on écrit l'en-tete
      annetelefile.println(F("Date,Index HC,Index HP,Index PV"));
   }
   else annetelefile = SD.open(fichier_annee, FILE_WRITE);
 
   if (annetelefile)                              // si le fichier est bien ouvert-> écriture
   {
      annetelefile.print(mois_jour);
      annetelefile.print(INDEX1);
      annetelefile.print(",");
      annetelefile.print(INDEX2);
      annetelefile.print(",");
      annetelefile.println(cpt2index);
      
      annetelefile.flush();                       // fermeture du fichier
      annetelefile.close();
    }
    else 
   {
      mymsg = SD.open("messages.log", FILE_WRITE);
      mymsg.print(date_heure);
      mymsg.println(F(" - Erreur ouverture fichier annuel"));
      mymsg.close();
   } 
}                                                 // End of Function fichier-annee()

///////////////////////////////////////////////////////////////////
// mise en forme Date & heure pour affichage ou enregistrement
///////////////////////////////////////////////////////////////////
void format_date_heure()
{ // date au format DATETIME de MySQL
  sprintf(date_heure,"%d-%02d-%02d %02d:%02d", annee, mois, jour, heure, minute);
}

///////////////////////////////////////////////////////////////////
// mise en forme Date & heure pour affichage ou enregistrement
///////////////////////////////////////////////////////////////////
void format_mois_jour()
{ // date au format DATETIME de MySQL
  sprintf(mois_jour,"%d-%02d-%02d,", annee, mois, jour);
}

///////////////////////////////////////////////////////////////////
// Convert normal decimal numbers to binary coded decimal
static uint8_t bin2bcd (uint8_t val) { 
  return val + 6 * (val / 10); 
}

///////////////////////////////////////////////////////////////////
// mise à l'heure de la RTC (DS1307)
///////////////////////////////////////////////////////////////////
void RTCsetTime(void)
{
  Wire.beginTransmission(104); // 104 is DS1307 device address (0x68)
  Wire.write(bin2bcd(0)); // start at register 0

  Wire.write(bin2bcd(seconde)); //Send seconds as BCD
  Wire.write(bin2bcd(minute)); //Send minutes as BCD
  Wire.write(bin2bcd(heure)); //Send hours as BCD
  Wire.write(bin2bcd(jour_semaine)); // dow
  Wire.write(bin2bcd(jour)); //Send day as BCD
  Wire.write(bin2bcd(mois)); //Send month as BCD
  Wire.write(bin2bcd(annee % 1000)); //Send year as BCD

  Wire.endTransmission();  
}

// Mise à jour manuelle de l'horloge
void maj_Horloge()
{
  digitalWrite(LEC_CPT1, LOW);
  if (!mem_reg_horloge) {
    switch (reg_horloge) { // debut de la structure 
    case 1: 
      Serial.print (F("Entrer Heure: "));
      break;
    case 2: 
      Serial.print (F("Entrer Minute: "));
      break;
    case 3: 
      Serial.print (F("Entrer Seconde: "));
      break;
    case 4: 
      Serial.print (F("Entrer Jour: "));
      break;
    case 5: 
      Serial.print (F("Entrer Mois: "));
      break;
    case 6: 
      Serial.print (F("Entrer Annee 20xx: "));
      break;
    }
    mem_reg_horloge = true;
  } 
  if (Serial.available()>0) { // si caractère dans la file d'attente

    //---- lecture du nombre reçu
    while (Serial.available() > 0) { // tant que buffer pas vide pour lire d'une traite tous les caractères reçus
      inByte = Serial.read(); // renvoie le 1er octet présent dans la file attente série (-1 si aucun) 
      if (((inByte > 47) && (inByte < 58)) || (inByte == 13 )) {
        ReceptionOctet = inByte - 48; // transforme valeur ASCII en valeur décimale
        if ((ReceptionOctet >= 0) && (ReceptionOctet <= 9)) ReceptionNombre = (ReceptionNombre*10) + ReceptionOctet; 
        // si valeur reçue correspond à un chiffre on calcule nombre
      }
      else presence_teleinfo = -1;
    } // fin while
    if (inByte == 13){
      if ((ReceptionNombre > val_max[reg_horloge-1]) || (ReceptionNombre == -1)) {
        Serial.println(F("Erreur"));
        ReceptionNombre = 0;
        mem_reg_horloge = true;
      }
      else {
        switch (reg_horloge) { // debut de la structure 
        case 1: 
          heure = ReceptionNombre;
          break;
        case 2: 
          minute = ReceptionNombre;
          break;
        case 3: 
          seconde = ReceptionNombre;
          break;
        case 4: 
          jour = ReceptionNombre;
          break;
        case 5: 
          mois = ReceptionNombre;
          break;
        case 6: 
          annee = 2000 + ReceptionNombre;
          break;
        }

        mem_reg_horloge = false;
        ReceptionNombre = 0;
        ++reg_horloge;

        if (reg_horloge > 6) {
          RTCsetTime();
          Serial.println(F("Reglage heure OK - installer le cavalier pour la teleinfo"));
          digitalWrite(LEC_CPT1, HIGH);
        }
      }
    }
  }
}

