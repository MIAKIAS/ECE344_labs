#include "common.h"
#include <string.h>
#include <stdbool.h>


bool isInt(char* arg){
	int size = strlen(arg);
	for (int i = 0; i < size; i++){
		if ('0' > arg[i] || arg[i] > '9')
			return false;
	}
	return true;
}
int factorial(int arg){
	if (arg == 1) return 1;
	return arg * factorial(arg - 1);
}
int
main(int argc, char **argv)
{
	if (argc == 1 || !isInt(argv[1]) || atoi(argv[1]) == 0){
		printf("Huh?\n");
	} else if (atoi(argv[1]) > 12){
		printf("Overflow\n");
	} else{
		printf("%d\n", factorial(atoi(argv[1])));
	}
	return 0;
}
