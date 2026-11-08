#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#include "server2.h"
#include "commands.h"

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

/* ---- Fichier des pseudos ------------------------------------------ */

static int recherchePseudo(const char *pseudo)
{
   FILE *fichier = fopen("users.csv", "r");
   if(fichier == NULL)
   {
      return 0;   /* pas encore de fichier : aucun pseudo connu */
   }

   char ligne[BUF_SIZE];
   int trouve = 0;

   while(fgets(ligne, sizeof(ligne), fichier) != NULL)
   {
      char *pseudo_fichier = strtok(ligne, ",\n");
      if(pseudo_fichier != NULL && strcmp(pseudo_fichier, pseudo) == 0)
      {
         trouve = 1;
         break;
      }
   }

   fclose(fichier);
   return trouve;
}

static void enregistrerPseudo(const char *pseudo)
{
   FILE *fichier = fopen("users.csv", "a");
   if(fichier == NULL)
   {
      perror("Erreur d'ouverture du fichier users.csv");
      return;
   }
   fprintf(fichier, "%s,\n", pseudo);
   fclose(fichier);
}

/* ---- Connexion / déconnexion d'un client -------------------------- */

static int pseudo_valide(const char *name)
{
   size_t len = strlen(name);
   if(len == 0 || len >= NAME_SIZE) return 0;
   for(size_t i = 0; i < len; i++)
   {
      /* pas d'espace/caractère de contrôle (cassent /defi), pas de virgule (CSV) */
      if((unsigned char)name[i] <= ' ' || name[i] == ',') return 0;
   }
   return 1;
}

/* La première ligne envoyée par un client est son pseudo. -1 = refusé. */
static int register_client(Client *c, const char *name)
{
   if(!pseudo_valide(name))
   {
      send_to(c, "Pseudo invalide (1 à %d caractères, sans espace ni virgule).", NAME_SIZE - 1);
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
   free(c);

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
      free(clients[i]);
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