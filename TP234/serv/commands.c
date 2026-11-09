#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "commands.h"
#include "server2.h"
#include "game.h"

typedef struct
{
   const char *name;
   void (*fn)(Client *me, char *args);
   const char *usage;
   const char *help;
} Command;

static void cmd_help(Client *me, char *args);
static void cmd_clients(Client *me, char *args);
static void cmd_defi(Client *me, char *args);
static void cmd_accept(Client *me, char *args);
static void cmd_refuse(Client *me, char *args);
static void cmd_cancel(Client *me, char *args);
static void cmd_play(Client *me, char *args);
static void cmd_msg(Client *me, char *args);

/* Pour ajouter une fonctionnalité : une fonction cmd_xxx + une ligne ici. */
static const Command commands[] = {
   {"help",     cmd_help,    "",            "affiche cette aide"},
   {"clients",  cmd_clients, "",            "liste les joueurs en ligne"},
   {"defi",     cmd_defi,    " <pseudo>",   "défie un joueur"},
   {"accepter", cmd_accept,  "",            "accepte le défi reçu"},
   {"refuser",  cmd_refuse,  "",            "refuse le défi reçu"},
   {"annuler",  cmd_cancel,  "",            "retire le défi que vous avez lancé"},
   {"jouer",    cmd_play,    " <1-6>",      "joue une case de votre camp"},
   {"msg",      cmd_msg,     " <pseudo> <texte>", "message privé"},
};
#define NB_COMMANDS (sizeof commands / sizeof commands[0])

/* ------------------------------------------------------------------ */

static void cmd_help(Client *me, char *args)
{
   char out[BUF_SIZE];
   size_t len;
   (void)args;

   len = (size_t)snprintf(out, sizeof out, "Commandes disponibles :");
   for(size_t i = 0; i < NB_COMMANDS && len < sizeof out; i++)
   {
      len += (size_t)snprintf(out + len, sizeof out - len, "\n  /%s%s : %s",
                              commands[i].name, commands[i].usage, commands[i].help);
   }
   send_to(me, "%s", out);
}

static void cmd_clients(Client *me, char *args)
{
   char out[BUF_SIZE];
   size_t len;
   (void)args;

   len = (size_t)snprintf(out, sizeof out, "Joueurs en ligne :");
   for(int i = 0; i < client_count() && len < sizeof out; i++)
   {
      Client *c = client_at(i);
      if(c->name[0] == 0) continue;   /* pas encore enregistré */
      len += (size_t)snprintf(out + len, sizeof out - len, "\n  - %s%s%s",
                              c->name,
                              c == me ? " (vous)" : "",
                              c->game ? " [en partie]" : "");
   }
   send_to(me, "%s", out);
}

/* ---- Défi ---------------------------------------------------------- */

static void cmd_defi(Client *me, char *args)
{
   if(args == NULL)
   {
      send_to(me, "Usage : /defi <pseudo>");
      return;
   }

   Client *other = client_find(args);
   if(other == NULL || other == me)
   {
      send_to(me, "Tu ne peux pas défier ce joueur (fait un effort stp) !");
      return;
   }
   if(me->game || other->game)
   {
      send_to(me, "Un des deux joueurs est déjà en partie, sois patient.");
      return;
   }
   if(me->challenged || me->challenger || other->challenged || other->challenger)
   {
      send_to(me, "Un défi est déjà en cours pour l'un de vous deux, trop de défi tue le défi.");
      return;
   }
   if(me->observing || other->observing)
   {
      send_to(me, "Un des deux joueurs est en train d'observer une partie, pas de jaloux.");
      return;
   }

   me->challenged = other;
   other->challenger = me;
   send_to(me, "Défi envoyé à %s. (si vous regrettez, vous pouvez toujours /annuler pour le retirer)", other->name);
   send_to(other, "%s vous défie ! Tapez /accepter ou /refuser si vous avez peur de perdre.", me->name);
}

static void cmd_accept(Client *me, char *args)
{
   (void)args;
   Client *opp = me->challenger;
   if(opp == NULL)
   {
      send_to(me, "Personne ne vous a défié :(.");
      return;
   }

   Game *g = game_create(opp, me);
   if(g == NULL)
   {
      send_to(me, "Trop de parties en cours, réessayez plus tard.");
      return;
   }
   me->challenger = NULL;
   opp->challenged = NULL;

   send_to_game(g, "La partie commence : %s (rangée du haut) contre %s (rangée du bas) !",
                opp->name, me->name);
   send_to_game(g, "Le sort a désigné %s pour commencer. Profites-en !", g->players[g->state.turn]->name);
   game_send_board(g, 0);
}

static void cmd_refuse(Client *me, char *args)
{
   (void)args;
   Client *opp = me->challenger;
   if(opp == NULL)
   {
      send_to(me, "Personne ne vous a défié.");
      return;
   }
   opp->challenged = NULL;
   me->challenger = NULL;
   send_to(opp, "%s a refusé votre défi.", me->name);
   send_to(me, "Défi refusé.");
}

static void cmd_cancel(Client *me, char *args)
{
   (void)args;
   Client *other = me->challenged;
   if(other == NULL)
   {
      send_to(me, "Vous n'avez lancé aucun défi.");
      return;
   }
   other->challenger = NULL;
   me->challenged = NULL;
   send_to(other, "%s a retiré son défi.", me->name);
   send_to(me, "Défi annulé.");
}

/* ---- Partie -------------------------------------------------------- */

static void cmd_play(Client *me, char *args)
{
   Game *g = me->game;
   if(g == NULL)
   {
      send_to(me, "Vous n'êtes pas en partie. Défiez quelqu'un avec /defi <pseudo>.");
      return;
   }

   char *end = NULL;
   long n = args ? strtol(args, &end, 10) : 0;
   if(args == NULL || end == args || *end != 0 || n < 1 || n > 6)
   {
      send_to(me, "Usage : /jouer <1-6>");
      return;
   }

   int side = game_side(g, me);
   if(g->state.turn != side)
   {
      send_to(me, "C'est pas ton tour :( !");
      return;
   }

   /* Chaque joueur voit ses cases numérotées de 1 à 6 : on traduit en case absolue */
   int pit = side * 6 + (int)n - 1;
   if(!awale_legal(&g->state, pit))
   {
      send_to(me, "Coup illégal (case vide, ou il faut nourrir l'adversaire miam-miam).");
      return;
   }

   int captured = awale_play(&g->state, pit);
   if(captured > 0)
   {
      send_to_game(g, "%s joue la case %ld et capture %d graines.", me->name, n, captured);
   }
   else
   {
      send_to_game(g, "%s joue la case %ld.", me->name, n);
   }

   int over = awale_over(&g->state);
   game_send_board(g, over);
   if(over)
   {
      game_end(g);
   }
}

/* ---- Chat ---------------------------------------------------------- */

static void cmd_msg(Client *me, char *args)
{
   char *text = args ? strchr(args, ' ') : NULL;
   if(text == NULL)
   {
      send_to(me, "Usage : /msg <pseudo> <texte>");
      return;
   }
   *text++ = 0;
   while(*text == ' ') text++;

   Client *dst = client_find(args);
   if(dst == NULL || dst == me || *text == 0)
   {
      send_to(me, "Message impossible (pseudo inconnu ou texte vide).");
      return;
   }
   send_to(dst, "[privé] %s : %s", me->name, text);
   send_to(me, "[privé -> %s] %s", dst->name, text);
}

static void chat_all(Client *me, const char *text)
{
   for(int i = 0; i < client_count(); i++)
   {
      Client *c = client_at(i);
      if(c != me && c->name[0])
      {
         send_to(c, "%s : %s", me->name, text);
      }
   }
}

/* ---- Dispatcher ---------------------------------------------------- */

void commands_handle(Client *me, char *line)
{
   if(line[0] != '/')
   {
      chat_all(me, line);   /* du texte simple = chat, jamais un coup */
      return;
   }

   char *name = strtok(line + 1, " ");
   char *args = name ? strtok(NULL, "") : NULL;

   if(args)
   {
      while(*args == ' ') args++;
      size_t len = strlen(args);
      while(len > 0 && args[len - 1] == ' ') args[--len] = 0;
      if(*args == 0) args = NULL;
   }

   if(name)
   {
      for(size_t i = 0; i < NB_COMMANDS; i++)
      {
         if(strcmp(name, commands[i].name) == 0)
         {
            commands[i].fn(me, args);
            return;
         }
      }
   }
   send_to(me, "Commande invalide ! Tapez /help.");
}

void commands_on_disconnect(Client *c)
{
   if(c->challenged)
   {
      send_to(c->challenged, "%s s'est déconnecté : son défi est annulé.", c->name);
      c->challenged->challenger = NULL;
      c->challenged = NULL;
   }
   if(c->challenger)
   {
      send_to(c->challenger, "%s s'est déconnecté : votre défi est annulé.", c->name);
      c->challenger->challenged = NULL;
      c->challenger = NULL;
   }
   if(c->game)
   {
      game_forfeit(c->game, c);
   }
}