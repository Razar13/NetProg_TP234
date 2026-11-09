#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "game.h"
#include "server2.h"

/* Registre des parties en cours (créneau libre = NULL). */
static Game *games[MAX_GAMES];
static int next_id = 1;

static Game *game_find(int id)
{
   for(int i = 0; i < MAX_GAMES; i++)
   {
      if(games[i] && games[i]->id == id) return games[i];
   }
   return NULL;
}

Game *game_create(Client *challenger, Client *challenged)
{
   int slot = -1;
   for(int i = 0; i < MAX_GAMES; i++)
   {
      if(games[i] == NULL)
      {
         slot = i;
         break;
      }
   }
   if(slot < 0) return NULL;

   Game *g = calloc(1, sizeof *g);
   if(g == NULL) return NULL;

   g->id = next_id++;
   g->players[0] = challenger;
   g->players[1] = challenged;
   awale_init(&g->state, rand() % 2);   /* le serveur tire au sort qui commence */

   /* C'est ce lien qui fait que la partie n'appartient qu'à ces deux joueurs */
   challenger->game = g;
   challenged->game = g;
   games[slot] = g;
   return g;
}

int game_side(const Game *g, const Client *c)
{
   return g->players[0] == c ? 0 : 1;
}

void send_to_game(Game *g, const char *fmt, ...)
{
   char buf[BUF_SIZE];
   va_list ap;

   va_start(ap, fmt);
   vsnprintf(buf, sizeof buf, fmt, ap);
   va_end(ap);

   for(int i = 0; i < 2; i++)
   {
      send_to(g->players[i], "%s", buf);
   }

   for(int i = 0; i < g->nb_observers; i++)
   {
      send_to(g->observers[i], "%s", buf);
   }
}

void game_send_board(Game *g, int over)
{
   char board[256];
   const Awale *a = &g->state;
   awale_board_string(a, board, sizeof board);

   if(over)
   {
      send_to_game(g, "\n%s : %d\n%s\n%s : %d",
                   g->players[0]->name, a->score[0], board,
                   g->players[1]->name, a->score[1]);
   }
   else
   {
      send_to_game(g, "\n%s : %d\n%s\n%s : %d\nAu tour de %s (/jouer <1-6>)",
                   g->players[0]->name, a->score[0], board,
                   g->players[1]->name, a->score[1],
                   g->players[a->turn]->name);
   }
}

void game_list(Client *client)
{
   int count = 0;

   send_to(client, "Parties en cours :");
   for(int i = 0; i < MAX_GAMES; i++)
   {
      Game *g = games[i];
      if(g!= NULL)
      {
         send_to(client, "  %d | %s : %d vs %d : %s (%d observateurs)",
                  g->id, g->players[0]->name, g->state.score[0], g->state.score[1], g->players[1]->name, g->nb_observers);
         count++;
      }
   }
   if(count == 0)
   {
      send_to(client, "  Aucune partie en cours.");
   }

   send_to(client, "Fin de la liste.");
}

int game_observe(Client *client, int game_id)
{
   Game *g = game_find(game_id);
   if(g == NULL)
   {
      send_to(client, "Aucune partie avec l'ID %d.", game_id);
      return 0;
   }

   if(client->game!=NULL || client->observing!=NULL)
   {
      send_to(client, "Vous êtes déjà dans une partie ou en train d'observer une partie.");
      return 0;
   }

   if(g->nb_observers >= MAX_OBSERVERS)
   {
      send_to(client, "La partie %d a déjà trop d'observateurs.", game_id);
      return 0;
   }

   g->observers[g->nb_observers++] = client;
   client->observing = g;

   send_to(client, "Vous observez la partie %d : %s vs %s.",
            g->id, g->players[0]->name, g->players[1]->name);
   game_send_board(g, 0);

   return 1;
}

void game_remove_observer(Client *client)
{
   Game *g = client->observing;
   if(g == NULL) return;

   int index = -1;
   for(int i = 0; i < g->nb_observers; i++)
   {
      if(g->observers[i] == client)
      {
         index = i;
         break;
      }
   }
   if(index < 0) return;

   for(int i = index; i < g->nb_observers - 1; i++)
   {
      g->observers[i] = g->observers[i + 1];
   }
   g->observers[g->nb_observers - 1] = NULL;
   g->nb_observers--;

   client->observing = NULL;
}

/* Détache les deux joueurs, retire la partie du registre et la libère. */
static void game_destroy(Game *g)
{
   for(int i = 0; i < 2; i++)
   {
      g->players[i]->game = NULL;
   }
   for(int i = 0; i < MAX_GAMES; i++)
   {
      if(games[i] == g) games[i] = NULL;
   }
   free(g);
}

void game_end(Game *g)
{
   int w = awale_winner(&g->state);

   if(w < 0)
   {
      send_to_game(g, "Fin de la partie : égalité %d - %d !",
                   g->state.score[0], g->state.score[1]);
   }
   else
   {
      send_to_game(g, "Fin de la partie : %s gagne %d - %d !",
                   g->players[w]->name, g->state.score[w], g->state.score[1 - w]);
   }
   game_destroy(g);
}

void game_forfeit(Game *g, Client *loser)
{
   Client *winner = g->players[0] == loser ? g->players[1] : g->players[0];
   send_to(winner, "%s s'est déconnecté : vous gagnez par forfait !", loser->name);
   game_destroy(g);
}