#ifndef AWALE_H
#define AWALE_H

#include <stddef.h>

/*
 * Règles de l'Awalé :
 *
 * Cases numérotées 0..11 dans le sens du jeu (anti-horaire) :
 *   - cases 0 à 5  : camp du joueur 0
 *   - cases 6 à 11 : camp du joueur 1
 * Un joueur ne manipule que les numéros 1..6 de son camp
 */
typedef struct
{
   int board[12];
   int score[2];
   int turn;        /* 0 ou 1 : joueur qui doit jouer */
   int direction;   /* 0 = sens normal, 1 = sens inverse (pour observer la partie) */
} Awale;

/* Plateau initial (4 graines partout). first = joueur qui commence. */
void awale_init(Awale *a, int first, int direction);

/* 1 si le joueur dont c'est le tour peut jouer la case pit (0..11). */
int awale_legal(const Awale *a, int pit);

/* Joue la case pit (le coup doit être légal, la verif est faite avant). Change le tour.
   Renvoie le nombre de graines capturées. */
int awale_play(Awale *a, int pit);

/* 1 si la partie est finie (25 graines, ou plus aucun coup possible :
   dans ce cas chacun ramasse les graines de son camp). */
int awale_over(Awale *a);

/* 0 ou 1 = vainqueur, -1 = égalité. Appelé quand awale_over() vaut 1. */
int awale_winner(const Awale *a);

/* Dessine les deux rangées du plateau (sans noms ni scores). */
void awale_board_string(const Awale *a, char *out, size_t n);

#endif