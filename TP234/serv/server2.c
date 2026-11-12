#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#include "server2.h"
#include "game.h"
#include "commands.h"

#define USERS_FILE "users.csv"
#define TEMP_FILE  "temp.csv"
#define LINE_SIZE  4096   /* une ligne du CSV : pseudo;bio;ami1,ami2,... */

/* Clients alloués avec malloc : un Client* reste valide même quand on
   décale le tableau, c'est ce qui permet à game->players[] de pointer dessus. */
static Client *clients[MAX_CLIENTS];
static int nb_clients = 0;

static void init(void)
{
#ifdef WIN32
   WSADATA wsa;
   int err = WSAStartup(MAKEWORD(2, 2), &wsa);
   if(err < 0)
   {
      puts("WSAStartup failed !");
      exit(EXIT_FAILURE);
   }
#endif
}

static void end(void)
{
#ifdef WIN32
   WSACleanup();
#endif
}

/* ---- Registre des clients ----------------------------------------- */

Client *client_find(const char *name)
{
   for(int i = 0; i < nb_clients; i++)
   {
      if(clients[i]->name[0] && strcmp(clients[i]->name, name) == 0)
      {
         return clients[i];
      }
   }
   return NULL;
}

int client_count(void)
{
   return nb_clients;
}

Client *client_at(int i)
{
   return clients[i];
}

/* ---- Envoi --------------------------------------------------------- */

void send_to(Client *c, const char *fmt, ...)
{
   char buf[BUF_SIZE];
   va_list ap;
   int n;

   va_start(ap, fmt);
   n = vsnprintf(buf, sizeof buf - 1, fmt, ap);
   va_end(ap);
   if(n < 0) return;
   if(n > (int)sizeof buf - 2) n = (int)sizeof buf - 2;
   buf[n++] = '\n';   /* une ligne = un message */

   /* MSG_NOSIGNAL : écrire sur une socket fermée ne doit pas tuer le serveur.
      Si le client est parti, c'est la lecture (recv == 0) qui le détectera. */
   if(send(c->sock, buf, n, MSG_NOSIGNAL) < 0)
   {
      perror("send()");
   }
}

/* ---- Listes de noms (amis, demandes d'ami) ------------------------- */
/* Chaque nom est une copie (strdup) appartenant à la liste : plus aucun
   pointeur vers inbuf, vers un buffer local ou vers un autre Client. */

int name_list_contains(char **list, int n, const char *s)
{
   for(int i = 0; i < n; i++)
   {
      if(strcmp(list[i], s) == 0) return 1;
   }
   return 0;
}

int name_list_add(char ***list, int *n, const char *s)
{
   char *copy = strdup(s);
   if(copy == NULL)
   {
      perror("strdup()");
      return -1;
   }
   char **tmp = realloc(*list, (*n + 1) * sizeof(char *));
   if(tmp == NULL)
   {
      perror("realloc()");
      free(copy);
      return -1;
   }
   *list = tmp;
   (*list)[(*n)++] = copy;
   return 0;
}

/* Renvoie 1 si le nom a été retiré, 0 s'il n'y était pas. */
int name_list_remove(char ***list, int *n, const char *s)
{
   for(int i = 0; i < *n; i++)
   {
      if(strcmp((*list)[i], s) == 0)
      {
         free((*list)[i]);
         memmove(*list + i, *list + i + 1, (*n - i - 1) * sizeof(char *));
         (*n)--;
         if(*n == 0)
         {
            free(*list);
            *list = NULL;
         }
         return 1;
      }
   }
   return 0;
}

void name_list_free(char ***list, int *n)
{
   for(int i = 0; i < *n; i++)
   {
      free((*list)[i]);
   }
   free(*list);
   *list = NULL;
   *n = 0;
}

/* ---- Fichier des clients ------------------------------------------ */
/* Format d'une ligne : pseudo;bio;ami1,ami2,ami3
   On n'utilise PAS strtok : il saute les délimiteurs consécutifs, donc
   "pseudo;;" (bio vide) serait mal lu. */

typedef struct
{
   char name[NAME_SIZE];
   char bio[BUF_SIZE];
   char friends[LINE_SIZE];   /* "ami1,ami2" ou "" */
} UserRecord;

/* Découpe une ligne du CSV. Renvoie 0 si la ligne est inutilisable. */
static int parse_line(char *ligne, UserRecord *r)
{
   ligne[strcspn(ligne, "\r\n")] = 0;

   char *bio = NULL;
   char *amis = NULL;
   char *sep = strchr(ligne, ';');
   if(sep != NULL)
   {
      *sep = 0;
      bio = sep + 1;
      sep = strchr(bio, ';');
      if(sep != NULL)
      {
         *sep = 0;
         amis = sep + 1;
      }
   }

   snprintf(r->name, sizeof r->name, "%s", ligne);
   snprintf(r->bio, sizeof r->bio, "%s", bio ? bio : "");
   snprintf(r->friends, sizeof r->friends, "%s", amis ? amis : "");
   return r->name[0] != 0;
}

/* Ajoute (ajout = 1) ou retire (ajout = 0) un ami dans une chaîne "a,b,c".
   Pas de virgule finale, pas de doublon. */
static void friends_edit(char *csv, size_t size, const char *ami, int ajout)
{
   char old[LINE_SIZE];
   snprintf(old, sizeof old, "%s", csv);
   csv[0] = 0;
   size_t len = 0;

   char *p = old;
   while(p != NULL)
   {
      char *next = strchr(p, ',');
      if(next != NULL) *next++ = 0;

      if(*p && strcmp(p, ami) != 0 && len < size)
      {
         len += (size_t)snprintf(csv + len, size - len, "%s%s", len ? "," : "", p);
      }
      p = next;
   }
   if(ajout && len < size)
   {
      snprintf(csv + len, size - len, "%s%s", len ? "," : "", ami);
   }
}

static int lire_utilisateur(const char *pseudo, UserRecord *out)
{
   FILE *fichier = fopen(USERS_FILE, "r");
   if(fichier == NULL)
   {
      return 0;   /* pas encore de fichier : aucun pseudo connu */
   }

   char ligne[LINE_SIZE];
   int trouve = 0;
   while(!trouve && fgets(ligne, sizeof ligne, fichier) != NULL)
   {
      UserRecord r;
      if(parse_line(ligne, &r) && strcmp(r.name, pseudo) == 0)
      {
         if(out) *out = r;
         trouve = 1;
      }
   }
   fclose(fichier);
   return trouve;
}

/* Réécrit users.csv en modifiant la ligne de `pseudo` :
   - new_bio != NULL : remplace la bio
   - ami != NULL     : ajoute (ajout = 1) ou retire (ajout = 0) cet ami */
static void fichier_modifier(const char *pseudo, const char *new_bio,
                             const char *ami, int ajout)
{
   FILE *fichier = fopen(USERS_FILE, "r");
   if(fichier == NULL)
   {
      perror("Erreur d'ouverture du fichier users.csv");
      return;
   }

   FILE *fichierTemp = fopen(TEMP_FILE, "w");
   if(fichierTemp == NULL)
   {
      perror("Erreur d'ouverture du fichier temporaire");
      fclose(fichier);
      return;
   }

   char ligne[LINE_SIZE];
   while(fgets(ligne, sizeof ligne, fichier) != NULL)
   {
      UserRecord r;
      if(!parse_line(ligne, &r)) continue;

      if(strcmp(r.name, pseudo) == 0)
      {
         if(new_bio) snprintf(r.bio, sizeof r.bio, "%s", new_bio);
         if(ami) friends_edit(r.friends, sizeof r.friends, ami, ajout);
      }
      fprintf(fichierTemp, "%s;%s;%s\n", r.name, r.bio, r.friends);
   }

   fclose(fichier);
   fclose(fichierTemp);

   remove(USERS_FILE);
   rename(TEMP_FILE, USERS_FILE);
}

static int recherchePseudo(const char *pseudo)
{
   return lire_utilisateur(pseudo, NULL);
}

static void enregistrerPseudo(const char *pseudo)
{
   FILE *fichier = fopen(USERS_FILE, "a");
   if(fichier == NULL)
   {
      perror("Erreur d'ouverture du fichier users.csv");
      return;
   }
   fprintf(fichier, "%s;;\n", pseudo);
   fclose(fichier);
}

/* ---- Amis / bio ---------------------------------------------------- */

void ajouter_ami(Client *c, char *ami)
{
   if(name_list_contains(c->friends, c->nb_friends, ami))
   {
      return;   /* déjà ami */
   }

   if(name_list_add(&c->friends, &c->nb_friends, ami) < 0) return;

   /* L'ami n'est pas forcément en ligne : dans ce cas seul le fichier change. */
   Client *ami_client = client_find(ami);
   if(ami_client != NULL &&
      !name_list_contains(ami_client->friends, ami_client->nb_friends, c->name))
   {
      name_list_add(&ami_client->friends, &ami_client->nb_friends, c->name);
   }

   fichier_modifier(c->name, NULL, ami, 1);
   fichier_modifier(ami, NULL, c->name, 1);

   
}

void supprimer_ami(Client *c, char *ami)
{
   /* `ami` peut pointer dans inbuf : on ne garde rien de lui après le retour. */
   if(!name_list_remove(&c->friends, &c->nb_friends, ami)) return;

   Client *ami_client = client_find(ami);
   if(ami_client != NULL)
   {
      name_list_remove(&ami_client->friends, &ami_client->nb_friends, c->name);
   }

   fichier_modifier(c->name, NULL, ami, 0);
   fichier_modifier(ami, NULL, c->name, 0);

   Game *g = c->game;
   if(g != NULL && g->private)
   {
      game_remove_observer(ami_client);
   }
}

void modifier_bio(Client *c, const char *bio)
{
   /* ';' et retour à la ligne casseraient le format CSV */
   char propre[BUF_SIZE];
   snprintf(propre, sizeof propre, "%s", bio);
   for(char *p = propre; *p; p++)
   {
      if(*p == ';' || *p == '\n' || *p == '\r') *p = ' ';
   }

   snprintf(c->bio, sizeof c->bio, "%s", propre);
   fichier_modifier(c->name, propre, NULL, 0);
}

void mettre_a_jour_amis(Client *c)
{
   name_list_free(&c->friends, &c->nb_friends);

   UserRecord r;
   if(!lire_utilisateur(c->name, &r)) return;

   char *p = r.friends;
   while(p != NULL)
   {
      char *next = strchr(p, ',');
      if(next != NULL) *next++ = 0;

      if(*p && recherchePseudo(p))
      {
         name_list_add(&c->friends, &c->nb_friends, p);   /* copie */
      }
      p = next;
   }
}

void mettre_a_jour_bio(Client *c)
{
   UserRecord r;
   if(lire_utilisateur(c->name, &r))
   {
      snprintf(c->bio, sizeof c->bio, "%s", r.bio);
   }
}

/* ---- Connexion / déconnexion d'un client -------------------------- */

static int pseudo_valide(const char *name)
{
   size_t len = strlen(name);
   if(len == 0 || len >= NAME_SIZE) return 0;
   for(size_t i = 0; i < len; i++)
   {
      /* pas d'espace/caractère de contrôle (cassent /defi),
         pas de virgule ni de point-virgule (CSV) */
      if((unsigned char)name[i] <= ' ' || name[i] == ',' || name[i] == ';') return 0;
   }
   return 1;
}

/* La première ligne envoyée par un client est son pseudo. -1 = refusé. */
static int register_client(Client *c, const char *name)
{
   if(!pseudo_valide(name))
   {
      send_to(c, "Pseudo invalide (1 à %d caractères, sans espace, virgule ni point-virgule).", NAME_SIZE - 1);
      return -1;
   }
   if(client_find(name) != NULL)
   {
      send_to(c, "Ce pseudo est déjà connecté.");
      return -1;
   }

   strncpy(c->name, name, NAME_SIZE - 1);
   if(!recherchePseudo(c->name))
   {
      enregistrerPseudo(c->name);
   }
   else
   {
      mettre_a_jour_bio(c);
      mettre_a_jour_amis(c);
   }

   send_to(c, "Bienvenue %s ! Tapez /help pour la liste des commandes.", c->name);
   for(int i = 0; i < nb_clients; i++)
   {
      if(clients[i] != c && clients[i]->name[0])
      {
         send_to(clients[i], "%s s'est connecté.", c->name);
      }
   }
   return 0;
}

static void accept_client(SOCKET sock)
{
   SOCKADDR_IN csin = { 0 };
   socklen_t sinsize = sizeof csin;
   int csock = accept(sock, (SOCKADDR *)&csin, &sinsize);
   if(csock == SOCKET_ERROR)
   {
      perror("accept()");
      return;
   }

   Client *c = calloc(1, sizeof *c);
   if(c == NULL || nb_clients >= MAX_CLIENTS)
   {
      const char *msg = "Serveur plein.\n";
      send(csock, msg, strlen(msg), MSG_NOSIGNAL);
      closesocket(csock);
      free(c);
      return;
   }
   c->sock = csock;
   clients[nb_clients++] = c;
}

static void free_client(Client *c)
{
   name_list_free(&c->friends, &c->nb_friends);
   name_list_free(&c->friend_requests, &c->nb_friend_requests);
   free(c);
}

static void disconnect_client(Client *c)
{
   char name[NAME_SIZE];
   strncpy(name, c->name, sizeof name);

   if(c->name[0])
   {
      commands_on_disconnect(c);   /* défis annulés, forfait éventuel */
   }

   closesocket(c->sock);
   for(int i = 0; i < nb_clients; i++)
   {
      if(clients[i] == c)
      {
         memmove(clients + i, clients + i + 1, (nb_clients - i - 1) * sizeof(Client *));
         nb_clients--;
         break;
      }
   }
   free_client(c);

   if(name[0])
   {
      for(int i = 0; i < nb_clients; i++)
      {
         if(clients[i]->name[0])
         {
            send_to(clients[i], "%s s'est déconnecté.", name);
         }
      }
   }
}

/* Lit ce qui arrive sur la socket et traite chaque ligne complète.
   TCP est un flux : un recv peut contenir une demi-ligne ou plusieurs lignes.
   Renvoie -1 si le client doit être déconnecté. */
static int handle_client_input(Client *c)
{
   int room = (int)sizeof c->inbuf - c->inlen;
   if(room <= 0)
   {
      c->inlen = 0;   /* ligne démesurée sans '\n' : on la jette */
      room = (int)sizeof c->inbuf;
   }

   int n = recv(c->sock, c->inbuf + c->inlen, room, 0);
   if(n < 0) perror("recv()");
   if(n <= 0) return -1;
   c->inlen += n;

   char *start = c->inbuf;
   char *nl;
   while((nl = memchr(start, '\n', c->inbuf + c->inlen - start)) != NULL)
   {
      *nl = 0;
      if(nl > start && nl[-1] == '\r') nl[-1] = 0;

      if(c->name[0] == 0)
      {
         if(register_client(c, start) < 0) return -1;
      }
      else if(start[0] != 0)
      {
         commands_handle(c, start);
      }
      start = nl + 1;
   }

   c->inlen -= (int)(start - c->inbuf);
   memmove(c->inbuf, start, c->inlen);
   return 0;
}

/* ---- Boucle principale -------------------------------------------- */

static int init_connection(void)
{
   SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
   SOCKADDR_IN sin = { 0 };
   int yes = 1;

   if(sock == INVALID_SOCKET)
   {
      perror("socket()");
      exit(errno);
   }

   /* permet de relancer le serveur tout de suite après un arrêt */
   setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof yes);

   sin.sin_addr.s_addr = htonl(INADDR_ANY);
   sin.sin_port = htons(PORT);
   sin.sin_family = AF_INET;

   if(bind(sock,(SOCKADDR *) &sin, sizeof sin) == SOCKET_ERROR)
   {
      perror("bind()");
      exit(errno);
   }

   if(listen(sock, MAX_CLIENTS) == SOCKET_ERROR)
   {
      perror("listen()");
      exit(errno);
   }

   return sock;
}

static void end_connection(int sock)
{
   closesocket(sock);
}

static void app(void)
{
   SOCKET sock = init_connection();
   fd_set rdfs;

   srand((unsigned)time(NULL));   /* pour le tirage au sort du premier joueur */

   while(1)
   {
      int max = sock;

      FD_ZERO(&rdfs);
      FD_SET(STDIN_FILENO, &rdfs);
      FD_SET(sock, &rdfs);
      for(int i = 0; i < nb_clients; i++)
      {
         FD_SET(clients[i]->sock, &rdfs);
         if(clients[i]->sock > max) max = clients[i]->sock;
      }

      if(select(max + 1, &rdfs, NULL, NULL, NULL) == -1)
      {
         perror("select()");
         exit(errno);
      }

      /* quelque chose au clavier du serveur : on arrête */
      if(FD_ISSET(STDIN_FILENO, &rdfs))
      {
         break;
      }

      if(FD_ISSET(sock, &rdfs))
      {
         accept_client(sock);
      }

      /* on parcourt tous les clients : plusieurs peuvent avoir parlé */
      for(int i = 0; i < nb_clients; i++)
      {
         if(FD_ISSET(clients[i]->sock, &rdfs))
         {
            if(handle_client_input(clients[i]) < 0)
            {
               disconnect_client(clients[i]);
               i--;   /* le tableau s'est décalé */
            }
         }
      }
   }

   for(int i = 0; i < nb_clients; i++)
   {
      closesocket(clients[i]->sock);
      free_client(clients[i]);
   }
   nb_clients = 0;
   end_connection(sock);
}

int main(int argc, char **argv)
{
   (void)argc;
   (void)argv;

   init();

   app();

   end();

   return EXIT_SUCCESS;
}