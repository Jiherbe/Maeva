#ifndef __CONSOTELEINFO_H_INCLUDED
#define __CONSOTELEINFO_H_INCLUDED

#include "FctCommunes.h"

class ConsoTeleInfo
{
public:
  ConsoTeleInfo();
  boolean readTeleInfo();
#ifdef __DEBUG
  void displayTeleInfo();
#endif
  boolean recordTeleInfoOnMySQLServer(const byte);
  void sauveSD(const byte);
  
private:
  int IINST1;              // intensité instantanée en A
  int IINST2;              // intensité instantanée en A
  int IINST3;              // intensité instantanée en A
  int PAPP;               // puissance apparente en VA
  unsigned long HCHC;  // compteur Heures Creuses en W
  unsigned long HCHP;  // compteur Heures Pleines en W
  boolean ethernetIsOK;

  int decodeTrame(char *buff);
};

#endif // __CONSOTELEINFO_H_INCLUDED
