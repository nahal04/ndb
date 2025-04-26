#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <readline/readline.h>
#include <readline/history.h>

#define DATA_MAX 1024

enum {TEXT_KEY, TEXT_VALUE, NDB_END, NDB_ERR};
enum {NOOP, OP_GET, OP_SET, NDB_DATA, SYN_ERR};
enum {RES_OK, RES_ERR};

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

typedef struct {
	int ok;
	char *data;
} result_t;

typedef struct db_t {
	char *key;
	char *value;
	struct db_t *next;
} db_t;

db_t *db;

db_t *create_db(char *key, char *value) {
	db_t *db = (db_t *) malloc(sizeof(db_t));
	db->key = strdup(key);
	db->value = strdup(value);
	db->next = NULL;
	return db;
}

void init_db() {
	db = (db_t *) malloc(sizeof(db_t));
	db->key = NULL;
	db->value = NULL;
	db->next = NULL;
}

void insert_db(db_t *db, db_t *node) {
	db_t *temp = db->next;
	db->next = node;
	node->next = temp;
}

db_t *find_db(db_t *db, char *k) {
	db_t *p = db->next;

	while (p != NULL) {
		if (strcmp(p->key, k) == 0)
			return p;
		p = p->next;
	}

	return NULL;
}


result_t *get_db(char *k) {
	result_t *res = (result_t *) malloc(sizeof(result_t));
	if (db == NULL) {
		res->ok = RES_ERR;
		res->data = "No Database initialized";
	}
	
	db_t *res_db = find_db(db, k);
	if (res_db == NULL) {
		res->ok = RES_ERR;
		res->data = "Not found";
		return res;
	}
	
	res->ok = RES_OK;
	res->data = res_db->value;
	return res;
}

result_t *set_db(char *k, char *v) {
	result_t *res = (result_t *) malloc(sizeof(result_t));
	if (db == NULL) {
		res->ok = RES_ERR;
		res->data = "No Database initialized";
	}

	db_t *res_db = find_db(db, k);
	if (res_db == NULL) {
		res_db = create_db(k, v);
		insert_db(db, res_db);
	} else {
		free(res_db->value);
		res_db->value = strdup(v);
	}

	res->ok = RES_OK;
	res->data = k;
	return res;
}

result_t *create_result(int status, char *data) {
	result_t *res = malloc(sizeof(result_t));
	res->ok = status;
	res->data = data;
	return res;
}

result_t *eval(ast_t *ast) {
	if (ast->op == NOOP)
		return NULL;

	if (ast->op == NDB_DATA) {
		result_t *res = malloc(sizeof(result_t));
		res->ok = RES_OK;
		res->data = ast->data;
		return res;
	}

	if (ast->op == OP_GET) {
		result_t *key = eval(ast->key);
		if (key->ok == RES_ERR)
			return key;
		return get_db(key->data);
	}

	if (ast->op == OP_SET) {
		result_t *k = eval(ast->key);
		result_t *v = eval(ast->value);
		if (k->ok == RES_ERR)
			return k;
		if (v->ok == RES_ERR) 
			return v;
		return set_db(k->data, v->data);
	}

		result_t *res = malloc(sizeof(result_t));
		res->ok = RES_ERR;
		res->data = "Syntax error";
		return res;
}

void print_res(result_t *res) {
	if (res == NULL)
		return;
	if (res->ok == RES_OK) {
		printf("%s\n", res->data);
	} else {
		fprintf(stderr, "Error: %s\n", res->data);
	}
}

int main() {
	puts("NDB version 0.0.1");
	puts("Press Ctrl+C to exit");
	init_db();

	while (1) {
		char *input = readline("ndb> ");

		if (input && *input) add_history(input);
		
		token_stream *ts = lex(input);
		ast_t *ast = parse(ts);
		result_t *res = eval(ast);
		print_res(res);

		free(input);
	}
}

