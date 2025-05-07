#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <errno.h>
#include <readline/readline.h>
#include <readline/history.h>

#define DATA_MAX 1024
#define SUB_COMMAND_MAX 1024

enum {TEXT_KEY, TEXT_VALUE, NDB_END, KEY_INDEX, SUB_COMMAND};
enum {NOOP, OP_GET, OP_SET, NDB_DATA, SYN_ERR};
enum {RES_OK, RES_ERR, RES_NOOP};

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

int parse_sub_command(char **s, char **p);




token_stream *lex(char *inp) {
	char *s = inp;
	char data[DATA_MAX];
	char *p = data;
	
	token_stream * ts = init_token_stream();

	while (isblank(*s))
	s++;
	while (*s != '\0') {
		while (*s != '\0' && *s != '<' && *s != '"' && *s != '[')
			s++;
		
		if (*s == '<') {
			s++;
			p = data;
			while (*s != '\0' && *s != '>') {
				if (*s == '(') {
					s++;
					int k = parse_sub_command(&s, &p);	
					if (k) {
						insert_end_token(ts);
						return ts;
					}
					s++;
				} else {
					*p++ = *s++;
				}
			}
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
			while (*s != '\0' && *s != '"') {
				if (*s == '(') {
					s++;
					int k = parse_sub_command(&s, &p);	
					if (k) {
						insert_end_token(ts);
						return ts;
					}
					s++;
				} else 
					*p++ = *s++;
			}
			if (*s == '\0') {
				insert_end_token(ts);
				return ts;
			}
			*p = '\0';
			token *t = create_token(TEXT_VALUE, data);
			insert_token(ts, t);
			s++;
		}

		if (*s == '[') {
			s++;
			p = data;
			while (*s != '\0' && *s != ']') {
				if (*s == '(') {
					s++;
					int k = parse_sub_command(&s, &p);	
					if (k) {
						insert_end_token(ts);
						return ts;
					}
					s++;
				} else 
					*p++ = *s++;
			}
			if (*s == '\0') {
				insert_end_token(ts);
				return ts;
			}
			*p = '\0';
			token *t = create_token(KEY_INDEX, data);
			insert_token(ts, t);
			s++;
		}

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
	ast->data = NULL;
	ast->key = ast->value = NULL;

	if (tsp->token->type == NDB_END) {
		ast->op = NOOP;
		return ast;
	}

	if (tsp->token->type == TEXT_KEY) {
		ast->key = data_ast(tsp->token->data);
		tsp = tsp->next;
		if (tsp->token->type  == NDB_END)
			ast->op = OP_GET;
		else if (tsp->token->type == KEY_INDEX) {
			ast->op = OP_GET;
			ast->data = tsp->token->data;
		}
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

db_t *db = NULL;

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


result_t get_db(char *k) {
	result_t res;
	if (db == NULL) {
		res.ok = RES_ERR;
		res.data = "No Database initialized";
	}
	
	db_t *res_db = find_db(db, k);
	if (res_db == NULL) {
		res.ok = RES_ERR;
		res.data = "Not found";
		return res;
	}
	
	res.ok = RES_OK;
	res.data = strdup(res_db->value);
	return res;
}

result_t set_db(char *k, char *v) {
	result_t res;
	if (db == NULL) {
		res.ok = RES_ERR;
		res.data = "No Database initialized";
	}

	db_t *res_db = find_db(db, k);
	if (res_db == NULL) {
		res_db = create_db(k, v);
		insert_db(db, res_db);
	} else {
		free(res_db->value);
		res_db->value = strdup(v);
	}

	res.ok = RES_OK;
	res.data = strdup(k);
	return res;
}

result_t get_value_indexed(result_t res, char *ind) {
	int i = (int) strtol(ind, NULL, 10);
	if (i < 0) {
		res.ok = RES_ERR;
		free(res.data);
		res.data = "Out of bound access";
	} 
	int comma_count = 0;
	char *p = res.data;
	while (*p != '\0' && comma_count < i) {
		if (*p++ == ',')
			comma_count++;
	}
	if (*p == '\0' && comma_count < i) {
		res.ok = RES_ERR;
		free(res.data);
		res.data = "Out of bound access";
		return res;
	}
	char *end = p;
	while (*end != '\0' && *end != ',')
		end++;

	*end = '\0';

	char *s = strdup(p);
	free(res.data);
	res.data = s;
	res.ok = RES_OK;
	return res;
} 

result_t eval(ast_t *ast) {
	result_t res;
	if (ast->op == NOOP) {
		res.ok = RES_NOOP;
		res.data = NULL;
		return res;
	}


	if (ast->op == NDB_DATA) {
		res.ok = RES_OK;
		res.data = strdup(ast->data);
		return res;
	}

	if (ast->op == OP_GET) {
		result_t key = eval(ast->key);
		if (key.ok == RES_ERR)
			return key;
		res = get_db(key.data);
		free(key.data);
		if (ast->data == NULL || res.ok == RES_ERR)
			return res;
		return get_value_indexed(res, ast->data);
	}

	if (ast->op == OP_SET) {
		result_t k = eval(ast->key);
		result_t v = eval(ast->value);
		if (k.ok == RES_ERR)
			return k;
		if (v.ok == RES_ERR) 
			return v;
		res = set_db(k.data, v.data);
		free(k.data);
		free(v.data);
		return res;
	}

		res.ok = RES_ERR;
		res.data = "Syntax error";
		return res;
}

void print_res(result_t res) {
	if (res.ok == RES_NOOP)
		return;
	if (res.ok == RES_OK) {
		printf("%s\n", res.data);
	} else {
		fprintf(stderr, "Error: %s\n", res.data);
	}
}

//cleanup functions
void cleanup_ts(token_stream *ts) {
	if (ts == NULL)
		return;
	if (ts->token->type == TEXT_KEY || ts->token->type == TEXT_VALUE)
		free(ts->token->data);
	free(ts->token);
	cleanup_ts(ts->next);
	free(ts);
}

void cleanup_ast(ast_t *ast) {
	if (ast->op == NOOP || ast->op == SYN_ERR || ast->op == NDB_DATA)
		return;
	if (ast->op == OP_SET)
		cleanup_ast(ast->value);
	cleanup_ast(ast->key);
	free(ast);
}

void cleanup_res(result_t res) {
	if (res.ok == RES_OK)
		free(res.data);
}

void cleanup(token_stream *ts, ast_t *ast, result_t res) {
	cleanup_ts(ts);
	cleanup_ast(ast);
	cleanup_res(res);
}

int parse_sub_command(char **s, char **p) {
	char temp[SUB_COMMAND_MAX];
	int i = 0;
	while (i < SUB_COMMAND_MAX && **s != ')' && **s != '\0') {
		temp[i++] = **s;
		(*s)++;
	}

	temp[i] = '\0';
	if (**s == ')') {
		token_stream *ts = lex(temp);
		ast_t *ast = parse(ts);
		result_t res = eval(ast);
		if (res.ok == RES_OK) {
			char *j = res.data;
			while (*j != '\0') {
				**p = *j++;
				(*p)++;
			}
			cleanup(ts, ast, res);
			return 0;
		}
		cleanup(ts, ast, res);
	}
	return 1;
}

void close_db(db_t *db) {
	if (db == NULL)
		return;
	free(db->key);
	free(db->value);
	close_db(db->next);
	free(db);
}

void init_db_from_file(char *filename) {
	init_db();
	int fd = open(filename, O_RDONLY);
	if (fd == -1) {
		perror("Read");
		return;
	}

	int len, total;

	char *k, *v, *s;
	ssize_t bytes;

	while (read(fd, &len, sizeof(int)) == sizeof(int)) {

		s = malloc(sizeof(char) * len);
		
		total = 0;
		while (total < len) {
			bytes = read(fd, s + total, len - total);
			if (bytes <= 0) {
				close(fd);
				return;
			}
			total += bytes;
		}
		
		k = s;
		v = s + strlen(k) + 1;
		db_t *new_db = create_db(k, v);	
		insert_db(db, new_db);
		free(s);
	}
	close(fd);
}

int commit_db_to_file(char *filename) {
	int fd = open(filename, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
	if (fd == -1) {
		perror("write");
		close_db(db);
		return -1;	
	}

	db_t *dbp = db->next;

	while (dbp != NULL) {
		int len = strlen(dbp->key) + strlen(dbp->value) + 2;
		
		write(fd, &len, sizeof(int));
		write(fd, dbp->key, strlen(dbp->key) + 1);
		write(fd, dbp->value, strlen(dbp->value) + 1);

		dbp = dbp->next;
	}
	close(fd);
	close_db(db);
	return 0;
}

typedef struct {
	char *infile;
	char *outfile;
	int servermode;
	char *port;

	char *error;
} args;

int parse_args(int argc, char *argv[], args *arg) {
	arg->infile = NULL;
	arg->outfile = NULL;
	arg->error = NULL;
	arg->servermode = 0;
	while (--argc) {
		if (strcmp(*(++argv), "--server") == 0) {
			if (!(--argc)) {
				arg->error = "No port number specified";
				return -1;
			}
			arg->servermode = 1;
			arg->port = strdup(*(++argv));
		}
		else if (strcmp(*argv, "--infile") == 0) {
			if (!(--argc)) {
				arg->error = "No input file specified";
				return -1;
			}

			arg->infile = strdup(*(++argv));
		}
		else if (strcmp(*argv, "--outfile") == 0) {
			if (!(--argc)) {
				arg->error = "No output file specified";
				return -1;
			}

			arg->infile = strdup(*(++argv));
		}
		else {
			arg->error = malloc(sizeof(char) * 64);
			sprintf(arg->error, "Unknown argument %s", *(argv));
			return -1;
		}
	}

	return 0;
}



int setup_server(char *port) {
	int sfd, s;
	struct addrinfo hints;
	struct addrinfo *result, *rp;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	s = getaddrinfo(NULL, port, &hints, &result);
	
	if (s != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		return -1;
	}

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sfd == -1)
			continue;
		if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0)
			break;
		
		close(sfd);
	}

	freeaddrinfo(result);

	if (rp == NULL) {
		fprintf(stderr, "Could not bind\n");
		close(sfd);
		return -1;
	}



	if (listen(sfd, 1) == -1) {
		perror("listen");
		return -1;
	}

	int client_fd = accept(sfd, NULL, NULL);
	if (client_fd == -1) {
		perror("accept");
		close(sfd);
		return -1;
	}

	return client_fd;
}

int main(int argc, char *argv[]) {
	puts("NDB version 0.2.0");
	puts("Press Ctrl+C to exit");

	args a;
	if (parse_args(argc, argv, &a) == -1) {
		fprintf(stderr, a.error);
		free(a.error);
		free(a.infile);
		free(a.outfile);
		exit(EXIT_FAILURE);
	}
	
	if (a.infile != NULL) init_db_from_file(a.infile);
	else init_db();
	
	int client_fd;
	char *input;
	char newline = '\n';
	if (a.servermode) {
		if ((client_fd = setup_server(a.port)) == -1)
			exit(EXIT_FAILURE);
		input = malloc(256);
	}

	ssize_t bytes;
		
	while (1) {
		if (a.servermode) {
			bytes = recv(client_fd, input, 256, 0);
			if (bytes == -1) {
				perror("recv");
				exit(EXIT_FAILURE);
			}

			input[bytes - 1] = '\0';

			if (strcmp(input, "q") == 0) {
				close(client_fd);
				free(input);
				break;
			}
		} else {

			input = readline("ndb> ");
			
			if (input && *input) add_history(input);
			if (strcmp(input, "q") == 0) break;
		}
		
		token_stream *ts = lex(input);
		ast_t *ast = parse(ts);
		result_t res = eval(ast);
		if (a.servermode) {
			if (res.ok == RES_OK || res.ok == RES_ERR) {
				bytes = send(client_fd, res.data, strlen(res.data), 0);
				if (bytes == -1) {
					perror("send");
					close(client_fd);
					exit(EXIT_FAILURE);
				}

			}
			send(client_fd, &newline, 1, 0); // for testing purposes
			cleanup(ts, ast, res);
		} else {
			print_res(res);
			cleanup(ts, ast, res);
			free(input);
		} 

	}

	if (a.outfile != NULL) commit_db_to_file(a.outfile);
	else close_db(db);
}

