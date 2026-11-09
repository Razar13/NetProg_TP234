#ifndef GAME_H
#define GAME_H

#include "types.h"

/* Crée une partie entre deux joueurs (challenger = camp du haut, cases 0-5).
   Le joueur qui commence est tiré au sort. NULL si trop de parties. */
Game *game_create(Client *challenger, Client *challenged);

/* 0 ou 1 : camp du client dans la partie. */
int game_side(const Game *g, const Client *c);

/* Envoie un message aux deux joueurs (et plus tard aux spectateurs). */
void send_to_game(Game *g, const char *fmt, ...) PRINTF_FMT(2, 3);

/* Envoie le plateau et les scores aux joueurs. over = 1 si la partie est finie. */
void game_send_board(Game *g, int over);

/* Annonce le résultat, détache les joueurs et libère la partie. */
void game_end(Game *g);

/* Un joueur a quitté : l'adversaire gagne par forfait, la partie est libérée. */
void game_forfeit(Game *g, Client *loser);

//affiche les parties en cours à un client
void game_list(Client *client);

//ajoute un client comme observateur d'une partie
int game_observe(Client *client, int game_id);

//retire un client de la partie qu'il observe
void game_remove_observer(Client *client);

#endif /* guard */