#ifndef SERVER_H
#define SERVER_H

#include "types.h"

int  name_list_contains(char **list, int n, const char *s);
int  name_list_add(char ***list, int *n, const char *s);
int  name_list_remove(char ***list, int *n, const char *s);
void name_list_free(char ***list, int *n);

/* Envoie une ligne formatée (printf) à un seul client. */
void send_to(Client *c, const char *fmt, ...) PRINTF_FMT(2, 3);

/* Registre des clients connectés. */
Client *client_find(const char *name);   /* par pseudo, NULL si absent */
int client_count(void);
Client *client_at(int i);
void modifier_bio(Client *c, const char *bio); /* permet de modifier la bio d'un client */
void ajouter_ami(Client *c, char *ami); /* permet d'ajouter un ami à la liste d'amis d'un client */
void supprimer_ami(Client *c, char *ami); /* permet de supprimer un ami de la liste d'amis d'un client */

void mettre_a_jour_bio(Client *c); /* permet de mettre à jour la bio d'un client à sa connexion */
void mettre_a_jour_amis(Client *c); /* permet de mettre à jour la liste d'amis d'un client à sa connexion */

#endif /* guard */