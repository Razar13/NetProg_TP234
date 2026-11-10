#ifndef TYPES_H
#define TYPES_H

#ifdef WIN32

#include <winsock2.h>
typedef int socklen_t;
#define MSG_NOSIGNAL 0

#elif defined (linux) || defined (__linux__)

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h> /* close */
#include <netdb.h> /* gethostbyname */
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket(s) close(s)
typedef int SOCKET;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;
typedef struct in_addr IN_ADDR;

#else

#error not defined for this platform

#endif

/* Permet au compilateur de vérifier les formats de send_to(...) */
#ifdef __GNUC__
#define PRINTF_FMT(a, b) __attribute__((format(printf, a, b)))
#else
#define PRINTF_FMT(a, b)
#endif

#define CRLF         "\r\n"
#define PORT         1977
#define MAX_CLIENTS  100
#define MAX_GAMES    50
#define MAX_OBSERVERS 100
#define BUF_SIZE     1024
#define NAME_SIZE    32

#include "awale.h"

typedef struct Client Client;
typedef struct Game Game;

struct Client
{
   SOCKET sock;
   char name[NAME_SIZE];   /* vide tant que le client n'a pas envoyé son pseudo */
   char inbuf[BUF_SIZE];   /* octets reçus pas encore découpés en lignes */
   int inlen;

   Game *game;             /* partie en cours, NULL sinon */
   Game *observing;        /* partie observée, NULL sinon */

   Client *challenger;     /* qui m'a défié (en attente de ma réponse), NULL sinon */
   Client *challenged;     /* qui j'ai défié (en attente de sa réponse), NULL sinon */
};

struct Game
{
   int id;
   Client *players[2];     /* players[0] possède les cases 0-5, players[1] les cases 6-11 */
   Awale state;

   Client *observers[MAX_CLIENTS]; /* NULL si pas d'observateur, sinon pointeur vers le client */
   int nb_observers;
};

#endif /* guard */