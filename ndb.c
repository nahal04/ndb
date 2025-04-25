#include <stdio.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>

int main() {
	puts("NDB version 0.0.1");
	puts("Press Ctrl+C to exit");

	while (1) {
		char *input = readline("ndb> ");

		if (input && *input)
			add_history(input);

		printf("Hello %s\n", input);

		free(input);
	}
}

