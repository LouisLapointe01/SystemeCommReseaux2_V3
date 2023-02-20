/*************************************************************
 * proto_tdd_v0 -  émetteur                                   *
 * TRANSFERT DE DONNEES  v3                                   *
 *                                                            *
 * Protocole sans contrôle de flux, sans reprise sur erreurs  *
 *                                                            *
 * Univ. de Toulouse III - Paul Sabatier         *
 **************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "application.h"
#include "couche_transport.h"
#include "services_reseau.h"
#define MAX_TRANSMISSION 20

/* =============================== */
/* Programme principal - émetteur  */
/* =============================== */
int main(int argc, char *argv[])
{
    int taille_fenetre;

    if (argc != 2)
    {
        fprintf(stderr, "usage: bin/emetteur $taille_fenetre\n");
        exit(1);
    }
    else if (atoi(argv[1]) < 1)
    {
        fprintf(stderr, "erreur: taille de la fenetre d'emission < 1");
        exit(1);
    }
    else
        taille_fenetre = atoi(argv[1]); // taille de la fenetre d'émission

    unsigned char message[MAX_INFO]; /* message de l'application */

    int taille_msg; /* taille du message */
    int borne_inf = 0;
    int curseur = 0, evenement;
    int nb_retransmission = 0;

    paquet_t ACKpaquet;
    paquet_t tab_paquet[SEQ_NUM_SIZE];

    init_reseau(EMISSION);

    printf("[TRP] Initialisation reseau : OK.\n");
    printf("[TRP] Debut execution protocole transport.\n");

    /* lecture de donnees provenant de la couche application */
    de_application(message, &taille_msg);

    /* tant que l'émetteur a des données à envoyer */
    while (taille_msg != 0 || (borne_inf != curseur))
    {

        if (dans_fenetre(borne_inf, curseur, taille_fenetre) && taille_msg > 0)
        {
            /* construction paquet */
            for (int i = 0; i < taille_msg; i++)
                tab_paquet[curseur].info[i] = message[i];

            tab_paquet[curseur].lg_info = taille_msg;
            tab_paquet[curseur].type = DATA;
            tab_paquet[curseur].num_seq = curseur;
            tab_paquet[curseur].somme_ctrl = generer_controle(tab_paquet[curseur]);

            vers_reseau(&tab_paquet[curseur]);

            if (curseur == borne_inf)
            {
                depart_temporisateur(0, 100);
            }
            curseur = (curseur + 1) % SEQ_NUM_SIZE;

            /* lecture de donnees provenant de la couche application */
            de_application(message, &taille_msg);
        }
        else
        {
            /*fct blocante*/
            evenement = attendre();
            if (evenement == PAQUET_RECU)
            {
                de_reseau(&ACKpaquet);

                if (verifier_controle(ACKpaquet) == 1 && dans_fenetre(borne_inf, ACKpaquet.num_seq, taille_fenetre))
                {
                    /* Décalage fenetre possible*/
                    borne_inf = (ACKpaquet.num_seq + 1) % SEQ_NUM_SIZE;
                    arreter_temporisateur(0);

                    if (borne_inf != curseur)
                        depart_temporisateur(0, 100);
                }
                /*sinon ACKpaquet ignoré (erroné ou hors fenetre)*/
            }
            else if (evenement == 0)
            {
                int i = borne_inf;
                depart_temporisateur(0, 100);
                nb_retransmission = 0;

                while (i != curseur)
                {
                    vers_reseau(&tab_paquet[i]);
                    i = (i + 1) % SEQ_NUM_SIZE;
                }
                nb_retransmission += 1;
                if (nb_retransmission > MAX_TRANSMISSION)
                {
                    fprintf(stderr, "[TRP]: nombre retransmissions atteint!\n");
                    return 0;
                }
            }
        }
    }

    printf("[TRP] Fin execution protocole transfert de donnees (TDD).\n");
    return 0;
}
