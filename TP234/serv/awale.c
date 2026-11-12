#include <stdio.h>
#include <string.h>

#include "awale.h"

/* Nombre de graines dans le camp d'un joueur. */
static int side_total(const int *board, int side)
{
   int total = 0;
   for(int i = side * 6; i < side * 6 + 6; i++)
   {
      total += board[i];
   }
   return total;
}

/* Sème les graines de la case pit sur board (sans capture).
   La case de départ est sautée si on fait un tour complet.
   Renvoie la dernière case semée. */
static int sow(const Awale *a, int *board, int pit)
{
   int n = board[pit];
   int j = pit;
   board[pit] = 0;

   while(n > 0)
   {
      if(a->direction == 0) j = (j + 1) % 12;
      else j = (j + 11) % 12;  /* sens inverse */
      
      if(j == pit)
      {
         continue;
      }
      board[j]++;
      n--;
   }
   return j;
}

void awale_init(Awale *a, int first, int direction)
{
   for(int i = 0; i < 12; i++)
   {
      a->board[i] = 4;
   }
   a->score[0] = 0;
   a->score[1] = 0;
   a->turn = first;
   a->direction = direction;
}

int awale_legal(const Awale *a, int pit)
{
   int opp = 1 - a->turn;

   if(pit < 0 || pit > 11) return 0;          /* hors plateau */
   if(pit / 6 != a->turn) return 0;           /* pas dans mon camp */
   if(a->board[pit] == 0) return 0;           /* case vide */

   /* Si l'adversaire n'a plus de graines, il faut le nourrir. */
   if(side_total(a->board, opp) == 0)
   {
      int tmp[12];
      memcpy(tmp, a->board, sizeof tmp);    /* on simule le coup dans un board temporaire (tmp) */
      sow(a,tmp, pit);
      if(side_total(tmp, opp) == 0) return 0;
   }
   return 1;
}

int awale_play(Awale *a, int pit)
{
   int opp = 1 - a->turn;
   int last = sow(a, a->board, pit);
   int captured = 0;

   /* Capture : seulement si on finit chez l'adversaire. On remonte en
      arrière tant que les cases contiennent 2 ou 3 graines. */
   if(last / 6 == opp)
   {
      int tmp[12];
      int k = last;
      int total = 0;
      memcpy(tmp, a->board, sizeof tmp);

      while(k >= opp * 6 && (tmp[k] == 2 || tmp[k] == 3))
      {
         total += tmp[k];
         tmp[k] = 0;
         k--;
      }

      /* Règle du "grand slam" : on ne peut pas affamer l'adversaire
         par une capture, le coup est joué mais sans capture. */
      if(total > 0 && side_total(tmp, opp) > 0)
      {
         memcpy(a->board, tmp, sizeof tmp);
         a->score[a->turn] += total;
         captured = total;
      }
   }

   a->turn = opp;
   return captured;
}

int awale_over(Awale *a)
{
   if(a->score[0] >= 25 || a->score[1] >= 25)
   {
      return 1;
   }

   for(int pit = a->turn * 6; pit < a->turn * 6 + 6; pit++)
   {
      if(awale_legal(a, pit))
      {
         return 0;
      }
   }

   /* Plus aucun coup possible : chacun ramasse les graines de son camp. */
   for(int side = 0; side < 2; side++)
   {
      a->score[side] += side_total(a->board, side);
      for(int i = side * 6; i < side * 6 + 6; i++)
      {
         a->board[i] = 0;
      }
   }
   return 1;
}

int awale_winner(const Awale *a)
{
   if(a->score[0] > a->score[1]) return 0;
   if(a->score[1] > a->score[0]) return 1;
   return -1;
}

/* Rangée du haut = joueur 0 (cases 1..6 de gauche à droite),
   rangée du bas = joueur 1 (cases 6..1 de gauche à droite).
   Les graines circulent donc dans le sens anti-horaire. */
void awale_board_string(const Awale *a, char *out, size_t n)
{
   const int *b = a->board;
   snprintf(out, n,
            "      1     2     3     4     5     6\n"
            "    [%2d]  [%2d]  [%2d]  [%2d]  [%2d]  [%2d]\n"
            "    [%2d]  [%2d]  [%2d]  [%2d]  [%2d]  [%2d]\n"
            "      6     5     4     3     2     1",
            b[0], b[1], b[2], b[3], b[4], b[5],
            b[11], b[10], b[9], b[8], b[7], b[6]);
}