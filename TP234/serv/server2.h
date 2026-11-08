#ifndef SERVER_H
#define SERVER_H

#include "types.h"

/* Envoie une ligne formatée (printf) à un seul client. */
void send_to(Client *c, const char *fmt, ...) PRINTF_FMT(2, 3);

/* Registre des clients connectés. */
Client *client_find(const char *name);   /* par pseudo, NULL si absent */
int client_count(void);
Client *client_at(int i);

#endif /* guard */