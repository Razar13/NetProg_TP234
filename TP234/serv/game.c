#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "game.h"
#include "server2.h"

/* Registre des parties en cours (créneau libre = NULL). */
static Game *games[MAX_GAMES];
static int next_id = 1;

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