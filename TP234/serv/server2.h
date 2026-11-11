#ifndef SERVER_H
#define SERVER_H

#include "types.h"

/* Envoie une ligne formatée (printf) à un seul client. */
void send_to(Client *c, const char *fmt, ...) PRINTF_FMT(2, 3);

/* Registre des clients connectés. */
Client *client_find(const char *name);   /* par pseudo, NULL si absent */
int client_count(void);
Client *client_at(int i);
void modifier_bio(Client *c, const char *bio); /* permet de modifier la bio d'un client */
void mettre_a_jour_bio(Client *c); /* permet de mettre à jour la bio d'un client à sa connexion */

#endif /* guard */