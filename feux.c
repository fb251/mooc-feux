/* Pour vérifier le fonctionnement j'ajoute le code ci-dessous 
   qui simule la présence de l'environnement Arduino alors que le 
   code est compilé sous GNU/Linux x86_64 :
    
   gcc -Wall -Werror -O3 -o feux feux.c

   Ça n'a pas d'intérêt pour vous, c'est juste pour que je puisse tester 
   sommairement que le code fonctionne. Le vrai code Arduino débute un 
   petit peu après. Donc c'est ma tambouille interne, n'en tenez pas 
   compte :) !   
*/    
 
/* 
 
// Code en mode scratch simulant très vaguement l'API Arduino et un 
// piéton pour mon Nunux
 
# include <stdio.h>
# include <stdlib.h>
# include <unistd.h>

# define HIGH	1
# define LOW	0
# define INPUT	0
# define OUTPUT 1	
 
char feu_a [3] = { 1, 2, 3 } ;
char feu_b [3] = { 4, 5, 6 } ;

extern int state ;
 
extern void setup (void) ;
 
extern void loop (void) ;

int digitalRead (int pin)
{
	(void) pin ;
	
	if ((! state) && ((rand () & 1) == 1))
	{
		printf ("\nBouton A !\n") ;
		return 1 ;
	}
		
	return 0 ;
}

void pinMode (int pin, int mode)
{
	(void) pin ;	
	(void) mode ;
}

void digitalWrite (int pin, int value)
{
	if (pin < 4)	// feu A
	{
		printf ("\n(state: %d) A: ", state) ;
	}
	else 			// feu B
	{
		printf ("\n(state: %d) B: ", state) ;
		pin -= 3 ;
	}
	
	switch (value)
	{
		case LOW :
			printf ("---\n") ;
			break ;
			
		case HIGH :
			switch (pin)
			{
				case 1 :
					printf ("vert\n") ;
					break ;
					
				case 2 :
					printf ("orange\n") ;
					break ;
					
				case 3 :
					printf ("rouge\n") ;
					break ;
					
				default:
					printf ("D'oh! (pin = %x, value = %x, state = %x)\n", pin, value, state) ;
					break ;					
			}
			break ;
			
		default :
			printf ("Y'a un bug: (pin = %x, value = %x, state = %x)\n", pin, value, state) ;
			break ;
	}
}

void delay (int duration)
{
	usleep (duration << 10) ;
}

int main (void)
{
	setup () ;
	while (1)
		loop () ;
		 
	return 0 ;	
}

*/

// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// Le code pour l'Arduino commence à partir d'ici.
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

// Code placé sous licence WTF 2.0 : http://wtflicense.com/
// Copyright 2014 © fb251
//
// Le carrefour est constitué de deux routes A et B, les deux feux sont
// synchrones, cela nous donne la matrice suivante pour le cycle 
// normal (j'ai repris les durées du TP de 3, 1 et 3 secondes) :
//
// Cycle    0-------------------------------1-----------------      ...
// État		0		1		2		3		0		1		2		...
// Temps	0s		3s		4s		7s		8s		11s		12s		...
// Feu A	vert	orange	rouge	rouge	vert	orange	rouge	...
// Feu B	rouge	rouge	vert	orange	rouge	rouge	vert	...
// Bouton A	X		-		-		-		X		-		-		...
//
// Le bouton piéton de la rue A ne doit être pris en compte que si le 
// feu A est vert (symbolisé par X dans la matrice).
//
// On en déduit qu'il y a un cycle de quatre états à gérer mais chaque 
// cycle n'est pas de durée égale. De plus l'appui sur BUTTON_A va 
// raccourcir la durée du feu A à l'état vert, ce qui revient à dire
// que l'on va passer plus rapidement de l'état 0 (feu A vert, 
// feu B rouge) à l'état 1 (feu A orange, feu B rouge). Le cycle sera 
// donc modifié pour donner la priorité à l'événement bouton A appuyé 
// ce qui implique que l'on doit pouvoir interrompre la temporisation
// de 3s pendant l'état 0. On va utiliser pour cela un automate à états 
// finis qui fera office de chef d'orchestre pour synchroniser tout ce 
// petit monde avec du polling pour détecter l'événement bouton appuyé 
// tout en restant en mode synchrone.
//
// Quelques précautions doivent être prises en particulier avec le 
// bouton A qui ne doit être pris en compte qu'une fois et uniquement 
// pendant l'état 0.
//
// Nous allons utiliser la méthode la plus adaptée compte tenu des 
// possibilités réduites de l'environnement Arduino. En l'occurrence
// tout sera synchronisé en fonction d'une variable d'état (state) que
// nous utiliserons pour indexer des tableaux contenant les valeurs
// pertinentes. Ce n'est ni plus ni moins que la formulation 
// informatique de la matrice ci-dessus. Comme il n'est pas possible de 
// faire des types structurés avec Arduino nous utilisons plusieurs 
// tableaux préfixés par dfa_ (deterministic finite automata) mais d'un 
// point de vue conceptuel il faut les voir comme une seule structure 
// de données.
//
// ATTENTION : un index commence à 0 donc les quatres états sont 
// numérotés comme suit : 0, 1, 2 et 3. Par ailleurs il y a souvent 
// le test "if (x)" qui revient à écrire "if (x != 0)" mais par 
// commodité on utilise la forme courte qui revient à dire qu'une 
// condition évaluée à zéro est fausse et que toute autre valeur est
// vraie.


# define STATES				4		// nombre d'états
# define LEDS				3		// nombre de leds pour un feu
# define BUTTON_A_PIN		10		// broche où est branché le bouton A

# define NO_EVENT			0		// aucun événement particulier
# define EVENT_BUTTON_A		1		// le bouton A a été appuyé


// Les broches qui pilotent les leds du feu A

int led_a [LEDS] =
{
	1,			// vert
	2,			// orange
	3			// rouge
} ;


// Les broches qui pilotent les leds du feu B

int led_b [LEDS] =
{
	4,			// vert
	5,			// orange
	6			// rouge
} ;


// Durée normale des cycles

int dfa_delay [STATES] = 
{ 
	3000,		// délai normal entre l'état 0 et l'état 1
	1000,		// délai normal entre l'état 1 et l'état 2
	3000,		// délai normal entre l'état 2 et l'état 3
	1000		// délai normal entre l'état 3 et l'état 0
} ;	


// Les états des feux A et B : chaque élément désigne l'index de la led 
// du feu à activer. 0 indique la led verte. On parle de double 
// indirection car on utilise un adressage avec plusieurs tableaux 
// imbriqués pour récupérer la valeur dont on a besoin (en l'occurrence
// la broche de la led à piloter). Cela va s'utiliser comme ceci :
// 
// digitalWrite (led_a [dfa_state_a [state]], HIGH) ;
//
// C'est la partie un peu subtile du code pour qui n'a pas l'habitude, 
// mais ça se décompose très facilement sous la forme suivante :
//
//  // Quelle doit être la couleur du feu A ?
//  color = dfa_state_a [state] ;
//  // Quelle est la broche qui correspond à la led du feu A à allumer ?
//  digitalWrite (led_a [color], HIGH) ;

int dfa_state_a [STATES] =
{
	0,			// vert	
	1,			// orange	
	2,			// rouge
	2			// rouge	
} ;


// Idem pour le feu B mais avec un déphasage de 180°

int dfa_state_b [STATES] =
{
	2,			// rouge
	2,			// rouge
	0,			// vert
	1			// orange	
} ;


// On indique pour le bouton A à quel moment il est autorisé

int dfa_button_a [STATES] =
{
	1,			// valide uniquement pendant l'état 0
	0,			// inhibé pour les autres états
	0,
	0
} ;


// La variable qui maintient l'état courant

int state = 0 ;		


// L'initialisation est très simple : mettre les leds en output et le 
// bouton en input.

void setup ()
{
	int i ;
	
	for (i = 0 ; i < LEDS ; i ++)
	{
		pinMode (led_a [i], OUTPUT) ;
		pinMode (led_b [i], OUTPUT) ;		
	}
	
	pinMode (BUTTON_A_PIN, INPUT) ;
}


// La fonction qui suit remplace la fonction delay() et va permettre 
// d'intercepter des événements asynchrones en vérifiant à 
// intervalle régulier si quelque chose doit être traité en priorité. 
// On parle de technique de polling.
// wait() va donc soit respecter la durée d'un cycle normal, soit 
// provoquer un changement d'état anticipé ce qui colle parfaitement 
// avec le problème des feux piéton.
// Note 1 : timeToGo doit etre inférieur à 32768 (32 secondes).
// Note 2 : on renvoie une valeur qui dans cet exemple n'est pas 
// exploitée ; dans un autre contexte cela peut être une information
// très utile.

int wait (int timeToGo)
{
	// On vérifie qu'il y a bien un délai minimal d'au moins 0,1 s
	if (timeToGo < 100)
		timeToGo = 100 ;
		
	// Boucle de polling toutes les 0,1s pendant timeToGo
	while (timeToGo > 0)
	{
		// Si un événement *valide* est détecté on arrête la 
		// boucle de polling pour remonter prioritairement
		// l'information.
		
		// Bouton A autorisé à ce moment ?
		if (dfa_button_a [state])
		{
			if (digitalRead (BUTTON_A_PIN))
				return EVENT_BUTTON_A ;
		}
		
		delay (100) ;
		// 0,1 s de moins à attendre
		timeToGo -= 100 ;
	}
	
	// Aucun événement particulier détecté pendant l'attente
	return NO_EVENT ;
}


// Boucle principale avec polling

void loop ()
{
	// 0. Si l'état invalide 4 (vu que ce serait le cinquième et que 
	// l'on en a que quatre) a été atteint on se replace à l'état 0
	// puisque c'est en fait un nouveau cycle qui débute
	if (state == 4)
		state = 0 ;

	// 1. On allume les feux selon l'état courant
	digitalWrite (led_a [dfa_state_a [state]], HIGH) ;
	digitalWrite (led_b [dfa_state_b [state]], HIGH) ;
	
	// 2. On attend pendant la durée normale avant la transition vers 
	// l'état suivant *OU* qu'un événement permis pendant cet état se 
	// produise
	wait (dfa_delay [state]) ;
	
	// 3. On éteint les feux (avec la persistance rétiniène ça ne 
	// se voit pas vu que l'on va reboucler quasi-instantanément sur
	// l'état suivant)
	digitalWrite (led_a [dfa_state_a [state]], LOW) ;
	digitalWrite (led_b [dfa_state_b [state]], LOW) ;
	
	// 4. On passe à l'état suivant avant de reboucler
	state ++ ;	
}
