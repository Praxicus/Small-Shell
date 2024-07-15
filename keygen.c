#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main (int argc, char *argv[]) 
{
	// seed random number generator
	time_t t;
	srand((unsigned) time(&t));

	int i = 0;
	int length = atoi(argv[1]);
	char char_list[28] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";

	// create random characters and print them one-by-one
	while (i < length) 
	{
		int rand_num = rand() % 27;
		char new_char = char_list[rand_num];

		printf("%c", new_char);
		i++;
	}
	printf("\n\0");
	return 0;
}