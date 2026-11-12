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
static void cmd_parties(Client *me, char *args);
static void cmd_observer(Client *me, char *args);
static void cmd_bio(Client *me, char *args);
static void cmd_profil(Client *me, char *args);
static void cmd_ami(Client *me, char *args);
static void cmd_supprimer(Client *me, char *args);
static void cmd_accept_ami(Client *me, char *args);
static void cmd_refuser_ami(Client *me, char *args);
static void cmd_observer_mode(Client *me, char *args);

/* Pour ajouter une fonctionnalité : une fonction cmd_xxx + une ligne ici. */
static const Command commands[] = {
   {"help",     cmd_help,    "",            "affiche cette aide"},
   {"clients",  cmd_clients, "",            "liste les joueurs en ligne"},
   {"parties",  cmd_parties, "",            "liste les parties en cours"},
   {"observer", cmd_observer," <id>",       "observe une partie en cours"},
   {"defi",     cmd_defi,    " <pseudo>",   "défie un joueur"},
   {"accepter", cmd_accept,  " <direction>",            "accepte le défi reçu et indique le sens de jeu (0 = horaire, 1 = antihoraire)"},
   {"refuser",  cmd_refuse,  "",            "refuse le défi reçu"},
   {"annuler",  cmd_cancel,  "",            "retire le défi que vous avez lancé"},
   {"jouer",    cmd_play,    " <1-6>",      "joue une case de votre camp"},
   {"msg",      cmd_msg,     " <pseudo> <texte>", "message privé"},
   {"bio",      cmd_bio,     " <texte>",    "met à jour votre bio"},
   {"profil",   cmd_profil,  " <pseudo>",   "affiche le profil d'un joueur"},
   {"ami",      cmd_ami,     " <pseudo>",   "envoie une demande d'ami à un joueur"},
   {"accepter_ami", cmd_accept_ami, " <pseudo>", "accepte une demande d'ami"},
   {"refuser_ami", cmd_refuser_ami, " <pseudo>", "refuse une demande d'ami"},
   {"supprimer",cmd_supprimer," <pseudo>",  "supprime un joueur de votre liste d'amis"},
   {"observer_mode", cmd_observer_mode, " <mode>", "change le mode de la partie en cours (0 = publique ou 1 = privée)"},
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

static void cmd_parties(Client *me, char *args)
{
   (void)args;
   game_list(me);
}

static void cmd_observer(Client *me, char *args)
{
   if(args == NULL)
   {
      send_to(me, "Usage : /observer <id>");
      return;
   }

   int game_id = atoi(args);
   if(game_id <= 0 || game_id > MAX_GAMES)
   {
      send_to(me, "ID de partie invalide (doit être entre 1 et %d).", MAX_GAMES);
      return;
   }

   game_observe(me, game_id);
}

static void cmd_observer_mode(Client *me, char *args)
{
   if(me->game == NULL)
   {
      send_to(me, "Vous n'êtes pas dans une partie.");
      return;
   }

   if(args == NULL)
   {
      send_to(me, "Usage : /observer_mode <mode>");
      return;
   }

   int mode = atoi(args);
   if(mode != 0 && mode != 1)
   {
      send_to(me, "Mode invalide (doit être 0 ou 1).");
      return;
   }

   me->game->private = mode;
   send_to(me, "Mode de la partie changé en %s.", mode ? "privée" : "publique");

   Game *g = me->game;
   for(int i = 0; i < g->nb_observers; i++)
   {
      Client *observer = g->observers[i];
      if(mode == 1)   /* partie privée : on retire les observateurs qui ne sont pas amis avec les joueurs */
      {
         int is_friend = 0;
         for(int j = 0; j < g->players[0]->nb_friends; j++)
         {
            if(strcmp(observer->name, g->players[0]->friends[j]) == 0)
            {
               is_friend = 1;
               break;
            }
         }
         for(int j = 0; j < g->players[1]->nb_friends; j++)
         {
            if(strcmp(observer->name, g->players[1]->friends[j]) == 0)
            {
               is_friend = 1;
               break;
            }
         }
         if(!is_friend)
         {
            send_to(observer, "La partie %d est maintenant privée : vous n'êtes pas ami avec les joueurs, vous ne pouvez plus observer.", g->id);
            game_remove_observer(observer);
            i--;   /* on a retiré un observateur, donc on reste sur le même index pour le prochain tour de boucle */
         }
      }
      else   /* partie publique : on peut laisser les observateurs */
      {
         send_to(observer, "La partie %d est maintenant publique : vous pouvez continuer à observer.", g->id);
      }
   }
}

static void cmd_bio(Client *me, char *args)
{
   if(args == NULL)
   {
      send_to(me, "Usage : /bio <texte>");
      return;
   }

   /* modifier_bio nettoie le texte (';' et retours à la ligne), met à jour
      me->bio et réécrit le fichier. */
   modifier_bio(me, args);
   send_to(me, "Bio mise à jour.");
}

/* ---- Liste d'amis ------------------------------------------------- */
/* friends et friend_requests sont des tableaux de char* possédés par le
   client (chaque nom est une copie). On compare toujours avec strcmp. */

static void cmd_ami(Client *me, char *args)
{
   if(args == NULL)
   {
      send_to(me, "Usage : /ami <pseudo>");
      return;
   }

   Client *other = client_find(args);
   if(other == NULL || other == me)
   {
      send_to(me, "Tu ne peux pas ajouter ce joueur à ta liste d'amis (fait un effort stp) !");
      return;
   }

   if(name_list_contains(me->friends, me->nb_friends, other->name))
   {
      send_to(me, "%s est déjà dans votre liste d'amis.", other->name);
      return;
   }

   if(name_list_contains(other->friend_requests, other->nb_friend_requests, me->name))
   {
      send_to(me, "Vous avez déjà envoyé une demande d'ami à %s.", other->name);
      return;
   }

   if(name_list_add(&other->friend_requests, &other->nb_friend_requests, me->name) < 0)
   {
      send_to(me, "Erreur serveur, réessayez plus tard.");
      return;
   }

   send_to(me, "Demande d'ami envoyée à %s.", other->name);
   send_to(other, "%s vous demande en ami ! Tapez /accepter_ami <pseudo> ou /refuser_ami <pseudo> si vous avez peur de lui.", me->name);
}

static void cmd_supprimer(Client *me, char *args)
{
   if(args == NULL)
   {
      send_to(me, "Usage : /supprimer <pseudo>");
      return;
   }

   /* L'ami peut être hors ligne : on se base sur la liste, pas sur client_find. */
   if(!name_list_contains(me->friends, me->nb_friends, args))
   {
      send_to(me, "Ce joueur n'est pas dans votre liste d'amis.");
      return;
   }

   char nom[NAME_SIZE];
   snprintf(nom, sizeof nom, "%s", args);   /* args vit dans inbuf, on garde une copie */

   supprimer_ami(me, nom);

   send_to(me, "%s a été supprimé de votre liste d'amis.", nom);
   Client *c = client_find(nom);
   if(c != NULL)
   {
      send_to(c, "%s vous a supprimé de sa liste d'amis car il ne vous aime plus.", me->name);
   }
}

static void cmd_accept_ami(Client *me, char *args)
{
   if(args == NULL)
   {
      send_to(me, "Usage : /accepter_ami <pseudo>");
      return;
   }

   char nom[NAME_SIZE];
   snprintf(nom, sizeof nom, "%s", args);

   if(!name_list_remove(&me->friend_requests, &me->nb_friend_requests, nom))
   {
      send_to(me, "%s ne vous a pas demandé en ami :(.", nom);
      return;
   }

   ajouter_ami(me, nom);
   send_to(me, "Vous avez accepté %s comme ami.", nom);

   Client *ami = client_find(nom);
   if(ami != NULL)
   {
      send_to(ami, "%s a accepté votre demande d'ami.", me->name);
   }
}

static void cmd_refuser_ami(Client *me, char *args)
{
   if(args == NULL)
   {
      send_to(me, "Usage : /refuser_ami <pseudo>");
      return;
   }

   char nom[NAME_SIZE];
   snprintf(nom, sizeof nom, "%s", args);

   if(!name_list_remove(&me->friend_requests, &me->nb_friend_requests, nom))
   {
      send_to(me, "%s ne vous a pas demandé en ami :(.", nom);
      return;
   }

   Client *ami = client_find(nom);
   if(ami != NULL)
   {
      send_to(ami, "%s a refusé votre demande d'ami.", me->name);
   }
   send_to(me, "Demande d'ami refusée.");
}

static void cmd_profil(Client *me, char *args)
{
   if(args == NULL)
   {
      send_to(me, "Usage : /profil <pseudo>");
      return;
   }

   Client *c = client_find(args);
   if(c == NULL)
   {
      send_to(me, "Aucun joueur en ligne avec ce pseudo.");
      return;
   }

   char liste_amis[BUF_SIZE];
   size_t len = 0;
   liste_amis[0] = 0;
   for(int i = 0; i < c->nb_friends && len < sizeof liste_amis; i++)
   {
      len += (size_t)snprintf(liste_amis + len, sizeof liste_amis - len, "%s%s",
                              c->friends[i], (i < c->nb_friends - 1) ? ", " : "");
   }
   if(c->nb_friends == 0)
   {
      snprintf(liste_amis, sizeof liste_amis, "(aucun ami :()");
   }

   send_to(me, "Profil de %s :\n  Bio : %s\n  Liste d'amis : %s\n  En partie : %s",
           c->name, c->bio[0] ? c->bio : "(aucune bio)",
           liste_amis,
           c->game ? "oui" : "non");
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
   send_to(other, "%s vous défie ! Tapez /accepter <0 (sens horaire)|1 (sens antihoraire)> ou /refuser si vous avez peur de perdre.", me->name);
}

static void cmd_accept(Client *me, char *args)
{
   Client *opp = me->challenger;
   if(opp == NULL)
   {
      send_to(me, "Personne ne vous a défié :(.");
      return;
   }

   char *end = NULL;
   int direction = args ? strtol(args, &end, 10) : 0;
   if(args == NULL || end == args || *end != 0 || (direction != 0 && direction != 1))
   {
      send_to(me, "Usage : /accepter <0|1>");
      return;
   }

   Game *g = game_create(opp, me, direction);
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
   if(c->observing)
   {
      game_remove_observer(c);
   }
   if(c->game)
   {
      game_forfeit(c->game, c);
   }
}