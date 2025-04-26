#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <readline/readline.h>
#include <readline/history.h>

#define DATA_MAX 1024

enum {TEXT_KEY, TEXT_VALUE, NDB_END, NDB_ERR};
enum {NOOP, OP_GET, OP_SET, NDB_DATA, SYN_ERR};

typedef struct {
	int type;
	char *data;
} token;

typedef struct token_stream {
	token *token;
	struct token_stream *next;
} token_stream;

token_stream *init_token_stream() {
	token_stream *ts = (token_stream *) malloc(sizeof(token_stream));
	ts->token = NULL;
	ts->next = NULL;
	return ts;
}

token *create_token(int type, char *data) {
	token *t = (token *) malloc(sizeof(token));
	t->type = type;
	if (data != NULL) {
		t->data = (char *) malloc(strlen(data) + 1);
		strcpy(t->data, data);
	} else t->data = NULL;
	return t;
}

int insert_token(token_stream *ts, token *t) {
	if (ts->token == NULL) {
		ts->token = t;
		return 0;
	} 
	token_stream *tsp = ts;
	while (tsp->next != NULL)
		tsp = tsp->next;
	token_stream *new_ts = (token_stream *) malloc(sizeof(token_stream));	
	new_ts->token = t;
	new_ts->next = NULL;
	tsp->next = new_ts;
	return 0;
}

void insert_end_token(token_stream *ts) {
	token *t = create_token(NDB_END, NULL);
	insert_token(ts, t);
}


token_stream *lex(char *inp) {
	char *s = inp;
	char data[DATA_MAX];
	char *p = data;

	token_stream * ts = init_token_stream();

	while (isblank(*s))
		s++;
	while (*s != '\0') {

		if (*s == '<') {
			s++;
			p = data;
			while (*s != '\0' && *s != '>')
				*p++ = *s++;
			if (*s == '\0') {
				insert_end_token(ts);
				return ts;
			}
			*p = '\0';
			token *t = create_token(TEXT_KEY, data);
			insert_token(ts, t);
			s++;
		}
		if (*s == '"') {
			s++;
			p = data;
			while (*s != '\0' && *s != '"')
				*p++ = *s++;
			if (*s == '\0') {
				insert_end_token(ts);
			}
			*p = '\0';
			token *t = create_token(TEXT_VALUE, data);
			insert_token(ts, t);
			s++;
		}

		while (isblank(*s))
			s++;
	}

	insert_end_token(ts);
	return ts;
}

typedef struct ast_t {
	int op;
	char *data;
	struct ast_t *key;
	struct ast_t *value;
} ast_t;

ast_t *data_ast(char *s) {
	ast_t *ast = (ast_t *) malloc(sizeof(ast_t));
	ast->op = NDB_DATA;
	ast->data = s;
	return ast;
}

ast_t *parse(token_stream *ts) {
	token_stream *tsp = ts;
	ast_t *ast = (ast_t *) malloc(sizeof(ast_t));

	if (tsp->token->type == NDB_END) {
		ast->op = NOOP;
		return ast;
	}

	if (tsp->token->type == TEXT_KEY) {
		ast->key = data_ast(tsp->token->data);
		tsp = tsp->next;
		if (tsp->token->type  == NDB_END)
			ast->op = OP_GET;
		else if (tsp->token->type == TEXT_VALUE) {
			ast->op = OP_SET;
			ast->value = data_ast(tsp->token->data);
		} else if (tsp->token->type == TEXT_KEY) {
			ast->op = OP_SET;
			ast->value = parse(tsp);
		}	
		return ast;
	}

	ast->op = SYN_ERR;
	return ast;
}

void print_ast(ast_t *ast) {
	if (ast->op == NOOP) {
		printf("NOOP");
		return;
	}
	if (ast->op == OP_GET) {
		printf("GET: ");
		printf("key: ");
		print_ast(ast->key);
		return;
	} 
	if (ast->op == OP_SET) {
		printf("SET: ");
		printf("key: ");
		print_ast(ast->key);
		printf("value: ");
		print_ast(ast->value);
		return;
	}
	if (ast->op == NDB_DATA) {
		printf("%s ", ast->data);
		return;
	}
	if (ast->op == SYN_ERR) {
		fprintf(stderr, "Error: syntax error");
		return;
	}
}

void print_ast_ln(ast_t *ast) {print_ast(ast); putchar('\n');}

int main() {
	puts("NDB version 0.0.1");
	puts("Press Ctrl+C to exit");

	while (1) {
		char *input = readline("ndb> ");

		if (input && *input) add_history(input);
		
		token_stream *ts = lex(input);
		ast_t *ast = parse(ts);
		print_ast_ln(ast);	

		free(input);
	}
}

