#ifndef COMMANDS_H
#define COMMANDS_H

#include "types.h"

/* Traite une ligne reçue d'un client enregistré :
   "/commande arguments" -> commande, sinon message de chat. */
void commands_handle(Client *me, char *line);

/* À appeler quand un client se déconnecte : annule ses défis,
   déclare forfait s'il était en partie. */
void commands_on_disconnect(Client *c);

#endif