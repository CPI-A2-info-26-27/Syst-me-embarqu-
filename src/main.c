#include <stdio.h>
#include "setup.h"

uint8_t fibonacci_asm(uint8_t iterations)
{
	uint32_t a = 1;
	uint32_t b = 2;
	uint32_t r = 0;

	for (uint8_t i = 0; i < iterations; i++)
	{
		uint32_t nouveau_a = 0;
		uint32_t nouveau_b = 0;

		/*
		 * Le code assembleur ci-dessous a été détérioré.
		 *
		 * Votre objectif :
		 * - calculer r = a + b ;
		 * - conserver uniquement les 8 bits de poids faible ;
		 * - préparer la prochaine itération avec a = b ;
		 * - préparer la prochaine itération avec b = r.
		 *
		 * Instructions disponibles :
		 * - ADDS : additionne deux registres
		 * - UXTB : conserve uniquement les 8 bits de poids faible
		 * - MOV : copie une valeur d'un registre vers un autre
		 */

		__asm volatile (
			"ADDS %[resultat], %[valeur_a], %[valeur_b]\n"
			"UXTB %[resultat], %[resultat]\n"
			"MOV %[nouveau_a], %[valeur_b]\n"
			"MOV %[nouveau_b], %[resultat]\n"
			: [resultat] "=&r" (r),
			  [nouveau_a] "=&r" (nouveau_a),
			  [nouveau_b] "=&r" (nouveau_b)
			: [valeur_a] "r" (a),
			  [valeur_b] "r" (b)
			: "cc"
		);

		a = nouveau_a;
		b = nouveau_b;

		printf("%lu\r\n", r);
	}

	return (uint8_t)r;
}

uint32_t fibonacci_c(uint8_t iterations)
{
	uint32_t a = 1;
	uint32_t b = 2;
	uint32_t r = 0;

	for (uint8_t i = 0; i < iterations; i++)
	{
		r = a + b;
		a = b;
		b = r;

		printf("%u\r\n", r);
	}

	return r;
}

int main(void)
{
	Global_Init();

	fibonacci_asm(11);
	fibonacci_c(11);
}