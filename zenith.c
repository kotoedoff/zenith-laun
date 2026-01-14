#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <math.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <pthread.h>
#include <unistd.h>
#include <dlfcn.h>
#include <dirent.h>
#include <fcntl.h>

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#ifdef HAVE_SDL2
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <GL/gl.h>
#endif

#ifdef HAVE_TCC
#include <libtcc.h>
#endif

#define MAX_LINE 8192
#define MAX_VARS 2048
#define MAX_FUNCTIONS 1024
#define MAX_TOKENS 1024
#define MAX_WINDOWS 64
#define MAX_MODULES 128
#define VERSION "0.4.0-beta"
#define MODULE_PATH "/usr/local/lib/zenith/modules"

typedef enum {
    TOK_EOF, TOK_NEWLINE, TOK_NUMBER, TOK_STRING, TOK_IDENT,
    TOK_PRINT, TOK_INPUT, TOK_IF, TOK_ELSE, TOK_ELIF, TOK_WHILE, TOK_FOR, TOK_IN, TOK_RANGE,
    TOK_FUNC, TOK_RETURN, TOK_BREAK, TOK_CONTINUE, TOK_CLASS, TOK_NEW, TOK_THIS, TOK_STATIC,
    TOK_SERVER, TOK_ROUTE, TOK_LISTEN, TOK_STOP, TOK_START, TOK_HTTP,
    TOK_LET, TOK_CONST, TOK_VAR, TOK_TRUE, TOK_FALSE, TOK_NULL, TOK_UNDEFINED,
    TOK_IMPORT, TOK_FROM, TOK_AS, TOK_EXPORT, TOK_MODULE, TOK_PACKAGE,
    TOK_FILE, TOK_OPEN, TOK_READ, TOK_WRITE, TOK_CLOSE, TOK_DELETE, TOK_EXISTS, TOK_MKDIR,
    TOK_ARRAY, TOK_DICT, TOK_APPEND, TOK_LENGTH, TOK_KEYS, TOK_VALUES, TOK_PUSH, TOK_POP,
    TOK_WINDOW, TOK_BUTTON, TOK_LABEL, TOK_ENTRY, TOK_SHOW, TOK_RENDER, TOK_CLOSE_WIN,
    TOK_RECT, TOK_CIRCLE, TOK_LINE, TOK_COLOR, TOK_PIXEL, TOK_DRAW, TOK_FILL, TOK_STROKE,
    TOK_HTML, TOK_CSS, TOK_STYLE, TOK_CLASS_CSS, TOK_ID,
    TOK_ENCRYPT, TOK_DECRYPT, TOK_HASH, TOK_SALT, TOK_SHA256, TOK_AES, TOK_RSA,
    TOK_COMPILE, TOK_BUILD, TOK_BINARY, TOK_EXECUTABLE, TOK_TCC, TOK_GCC,
    TOK_TRY, TOK_CATCH, TOK_THROW, TOK_FINALLY,
    TOK_ASYNC, TOK_AWAIT, TOK_PROMISE,
    TOK_TYPEOF, TOK_INSTANCEOF, TOK_DELETE_KW,
    TOK_LPAREN, TOK_RPAREN, TOK_LBRACE, TOK_RBRACE, TOK_LBRACKET, TOK_RBRACKET,
    TOK_COMMA, TOK_COLON, TOK_SEMICOLON, TOK_DOT, TOK_QUESTION, TOK_AT,
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_PERCENT, TOK_POWER,
    TOK_EQ, TOK_EQEQ, TOK_EQEQEQ, TOK_NEQ, TOK_NEQEQ, TOK_LT, TOK_GT, TOK_LTE, TOK_GTE,
    TOK_AND, TOK_OR, TOK_NOT, TOK_ARROW, TOK_PLUSEQ, TOK_MINUSEQ, TOK_STAREQ, TOK_SLASHEQ,
    TOK_PLUSPLUS, TOK_MINUSMINUS, TOK_BITAND, TOK_BITOR, TOK_BITXOR, TOK_BITNOT, TOK_LSHIFT, TOK_RSHIFT
} TokenType;

typedef struct Token {
    TokenType type;
    char *value;
    int line;
} Token;

typedef enum {
    VAL_NULL, VAL_NUMBER, VAL_STRING, VAL_BOOL, VAL_ARRAY, VAL_DICT, 
    VAL_FUNCTION, VAL_WINDOW, VAL_COMPILED, VAL_MODULE, VAL_UNDEFINED
} ValueType;

typedef struct Value Value;

typedef struct {
    char *name;
    Value *value;
    bool is_const;
    int scope;
} Variable;

typedef struct {
    char *name;
    char **params;
    int param_count;
    Token *body_tokens;
    int body_token_count;
} Function;

typedef struct {
    char *name;
    Variable *exports;
    int export_count;
} Module;

#ifdef HAVE_SDL2
typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_GLContext gl_context;
    int width;
    int height;
    bool use_opengl;
    bool running;
    SDL_Event event;
} ZenithWindow;
#endif

typedef struct Value {
    ValueType type;
    union {
        double number;
        char *string;
        bool boolean;
        struct {
            Value **items;
            int count;
            int capacity;
        } array;
        struct {
            char **keys;
            Value **values;
            int count;
            int capacity;
        } dict;
        Function *function;
#ifdef HAVE_SDL2
        ZenithWindow *window;
#endif
        void *compiled;
        Module *module;
    } data;
} Value;

Variable vars[MAX_VARS];
int var_count = 0;
Function funcs[MAX_FUNCTIONS];
int func_count = 0;
Module modules[MAX_MODULES];
int module_count = 0;
bool repl_mode = false;
int current_scope = 0;
// Globals for return handling
Value *return_val = NULL;
bool is_returning = false;

typedef struct {
    int port;
    char *root_dir;
    bool running;
    pthread_t thread;
} HTTPServer;

HTTPServer http_server = {0};

#ifdef HAVE_SDL2
ZenithWindow windows[MAX_WINDOWS];
int window_count = 0;
#endif


// Forward declarations
void execute_block(Token *tokens, int count, int *idx);
void execute_statement(Token *tokens, int count, int *idx);
Function *find_function(const char *name);

char *strdup_safe(const char *s) {
    if (!s) return NULL;
    char *d = malloc(strlen(s) + 1);
    if (d) strcpy(d, s);
    return d;
}


void trim(char *s) {
    if (!s) return;
    char *p = s;
    int l = strlen(p);
    while (l > 0 && isspace(p[l - 1])) p[--l] = 0;
    while (*p && isspace(*p)) ++p, --l;
    memmove(s, p, l + 1);
}

Value *create_value(ValueType type) {
    Value *v = calloc(1, sizeof(Value));
    v->type = type;
    if (type == VAL_ARRAY) {
        v->data.array.capacity = 16;
        v->data.array.items = calloc(16, sizeof(Value*));
        v->data.array.count = 0;
    } else if (type == VAL_DICT) {
        v->data.dict.capacity = 16;
        v->data.dict.keys = calloc(16, sizeof(char*));
        v->data.dict.values = calloc(16, sizeof(Value*));
        v->data.dict.count = 0;
    }
    return v;
}

void free_value(Value *v) {
    if (!v) return;
    if (v->type == VAL_STRING && v->data.string) {
        free(v->data.string);
    } else if (v->type == VAL_ARRAY) {
        for (int i = 0; i < v->data.array.count; i++) {
            free_value(v->data.array.items[i]);
        }
        free(v->data.array.items);
    } else if (v->type == VAL_DICT) {
        for (int i = 0; i < v->data.dict.count; i++) {
            free(v->data.dict.keys[i]);
            free_value(v->data.dict.values[i]);
        }
        free(v->data.dict.keys);
        free(v->data.dict.values);
    }
    free(v);
}

Value *copy_value(Value *v) {
    if (!v) return create_value(VAL_NULL);
    Value *copy = create_value(v->type);
    if (v->type == VAL_NUMBER) {
        copy->data.number = v->data.number;
    } else if (v->type == VAL_STRING) {
        copy->data.string = strdup_safe(v->data.string);
    } else if (v->type == VAL_BOOL) {
        copy->data.boolean = v->data.boolean;
    } else if (v->type == VAL_ARRAY) {
        copy->data.array.capacity = v->data.array.capacity;
        copy->data.array.count = v->data.array.count;
        copy->data.array.items = calloc(copy->data.array.capacity, sizeof(Value*));
        for (int i = 0; i < v->data.array.count; i++) {
            copy->data.array.items[i] = copy_value(v->data.array.items[i]);
        }
    } else if (v->type == VAL_DICT) {
        copy->data.dict.capacity = v->data.dict.capacity;
        copy->data.dict.count = v->data.dict.count;
        copy->data.dict.keys = calloc(copy->data.dict.capacity, sizeof(char*));
        copy->data.dict.values = calloc(copy->data.dict.capacity, sizeof(Value*));
        for (int i = 0; i < v->data.dict.count; i++) {
            copy->data.dict.keys[i] = strdup_safe(v->data.dict.keys[i]);
            copy->data.dict.values[i] = copy_value(v->data.dict.values[i]);
        }
    } else if (v->type == VAL_FUNCTION) {
        // Shallow copy function for now as it's immutable usually
        copy->data.function = v->data.function;
    } else if (v->type == VAL_MODULE) {
        copy->data.module = v->data.module;
    }
    return copy;
}

char *value_to_string(Value *v) {
    if (!v || v->type == VAL_NULL) return strdup("null");
    if (v->type == VAL_UNDEFINED) return strdup("undefined");
    
    static char buffer[1024];
    if (v->type == VAL_NUMBER) {
        if (v->data.number == (long)v->data.number) {
            snprintf(buffer, 1024, "%ld", (long)v->data.number);
        } else {
            snprintf(buffer, 1024, "%g", v->data.number);
        }
        return strdup(buffer);
    } else if (v->type == VAL_STRING) {
        return strdup_safe(v->data.string);
    } else if (v->type == VAL_BOOL) {
        return strdup(v->data.boolean ? "true" : "false");
    } else if (v->type == VAL_ARRAY) {
        char *result = strdup("[");
        for (int i = 0; i < v->data.array.count; i++) {
            char *item = value_to_string(v->data.array.items[i]);
            char *temp = malloc(strlen(result) + strlen(item) + 4);
            sprintf(temp, "%s%s%s", result, i > 0 ? ", " : "", item);
            free(result);
            free(item);
            result = temp;
        }
        char *temp = malloc(strlen(result) + 2);
        sprintf(temp, "%s]", result);
        free(result);
        return temp;
    }
    return strdup("unknown");
}

void set_var(const char *name, Value *value, bool is_const) {
    // Search backwards to find the nearest variable with the same name (shadowing or updating)
    // If found, update it. If not, create new in current scope.
    for (int i = var_count - 1; i >= 0; i--) {
        if (strcmp(vars[i].name, name) == 0) {
            // Found existing variable
            if (vars[i].is_const) {
                printf("Error: Cannot reassign constant '%s'\n", name);
                return;
            }
            free_value(vars[i].value);
            vars[i].value = copy_value(value);
            return;
        }
    }
    
    // Not found, create new in current scope
    if (var_count < MAX_VARS) {
        vars[var_count].name = strdup_safe(name);
        vars[var_count].value = copy_value(value);
        vars[var_count].is_const = is_const;
        vars[var_count].scope = current_scope;
        var_count++;
    }
}

Value *get_var(const char *name) {
    for (int i = var_count - 1; i >= 0; i--) {
        if (strcmp(vars[i].name, name) == 0) {
            return vars[i].value;
        }
    }
    return NULL;
}

Value *crypto_hash(const char *data, const char *algorithm) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)data, strlen(data), hash);
    
    char hex[SHA256_DIGEST_LENGTH * 2 + 1];
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(hex + (i * 2), "%02x", hash[i]);
    }
    hex[SHA256_DIGEST_LENGTH * 2] = 0;
    
    Value *v = create_value(VAL_STRING);
    v->data.string = strdup(hex);
    return v;
}

Value *crypto_encrypt_aes(const char *data, const char *key) {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    unsigned char iv[16];
    RAND_bytes(iv, 16);
    
    unsigned char key_hash[32];
    SHA256((unsigned char*)key, strlen(key), key_hash);
    
    unsigned char *ciphertext = malloc(strlen(data) + 32);
    int len, ciphertext_len;
    
    EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key_hash, iv);
    EVP_EncryptUpdate(ctx, ciphertext, &len, (unsigned char*)data, strlen(data));
    ciphertext_len = len;
    EVP_EncryptFinal_ex(ctx, ciphertext + len, &len);
    ciphertext_len += len;
    EVP_CIPHER_CTX_free(ctx);
    
    char *result = malloc((16 + ciphertext_len) * 2 + 1);
    for (int i = 0; i < 16; i++) {
        sprintf(result + (i * 2), "%02x", iv[i]);
    }
    for (int i = 0; i < ciphertext_len; i++) {
        sprintf(result + (16 * 2) + (i * 2), "%02x", ciphertext[i]);
    }
    free(ciphertext);
    
    Value *v = create_value(VAL_STRING);
    v->data.string = result;
    return v;
}

Value *crypto_decrypt_aes(const char *encrypted_hex, const char *key) {
    int hex_len = strlen(encrypted_hex);
    int data_len = hex_len / 2;
    unsigned char *data = malloc(data_len);
    
    for (int i = 0; i < data_len; i++) {
        sscanf(encrypted_hex + (i * 2), "%2hhx", &data[i]);
    }
    
    unsigned char iv[16];
    memcpy(iv, data, 16);
    
    unsigned char key_hash[32];
    SHA256((unsigned char*)key, strlen(key), key_hash);
    
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    unsigned char *plaintext = malloc(data_len);
    int len, plaintext_len;
    
    EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key_hash, iv);
    EVP_DecryptUpdate(ctx, plaintext, &len, data + 16, data_len - 16);
    plaintext_len = len;
    EVP_DecryptFinal_ex(ctx, plaintext + len, &len);
    plaintext_len += len;
    EVP_CIPHER_CTX_free(ctx);
    free(data);
    
    plaintext[plaintext_len] = 0;
    Value *v = create_value(VAL_STRING);
    v->data.string = strdup((char*)plaintext);
    free(plaintext);
    return v;
}

Value *crypto_generate_salt(int length) {
    unsigned char salt[length];
    RAND_bytes(salt, length);
    
    char *hex = malloc(length * 2 + 1);
    for (int i = 0; i < length; i++) {
        sprintf(hex + (i * 2), "%02x", salt[i]);
    }
    hex[length * 2] = 0;
    
    Value *v = create_value(VAL_STRING);
    v->data.string = hex;
    return v;
}

#ifdef HAVE_SDL2
Value *graphics_create_window(const char *title, int width, int height, bool use_opengl) {
    if (window_count >= MAX_WINDOWS) {
        return create_value(VAL_NULL);
    }
    
    static bool sdl_inited = false;
    if (!sdl_inited) {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
            printf("SDL Error: %s\n", SDL_GetError());
            return create_value(VAL_NULL);
        }
        sdl_inited = true;
    }
    
    ZenithWindow *win = &windows[window_count++];
    win->width = width;
    win->height = height;
    win->use_opengl = use_opengl;
    win->running = true;
    
    Uint32 flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
    if (use_opengl) {
        flags |= SDL_WINDOW_OPENGL;
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    }
    
    win->window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                    width, height, flags);
    
    if (!win->window) {
        printf("Window Error: %s\n", SDL_GetError());
        return create_value(VAL_NULL);
    }
    
    if (use_opengl) {
        win->gl_context = SDL_GL_CreateContext(win->window);
        if (!win->gl_context) {
            printf("GL Context Error: %s\n", SDL_GetError());
            SDL_DestroyWindow(win->window);
            return create_value(VAL_NULL);
        }
        SDL_GL_MakeCurrent(win->window, win->gl_context);
        glViewport(0, 0, width, height);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, width, height, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
    } else {
        win->renderer = SDL_CreateRenderer(win->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!win->renderer) {
            printf("Renderer Error: %s\n", SDL_GetError());
            SDL_DestroyWindow(win->window);
            return create_value(VAL_NULL);
        }
        SDL_SetRenderDrawBlendMode(win->renderer, SDL_BLENDMODE_BLEND);
    }
    
    Value *v = create_value(VAL_WINDOW);
    v->data.window = win;
    return v;
}

void graphics_poll_events(ZenithWindow *win) {
    while (SDL_PollEvent(&win->event)) {
        if (win->event.type == SDL_QUIT) {
            win->running = false;
        }
    }
}

void graphics_draw_rect(ZenithWindow *win, int x, int y, int w, int h, int r, int g, int b, int a) {
    if (!win || !win->running) return;
    
    if (win->use_opengl) {
        glColor4ub(r, g, b, a);
        glBegin(GL_QUADS);
        glVertex2i(x, y);
        glVertex2i(x + w, y);
        glVertex2i(x + w, y + h);
        glVertex2i(x, y + h);
        glEnd();
    } else {
        SDL_SetRenderDrawColor(win->renderer, r, g, b, a);
        SDL_Rect rect = {x, y, w, h};
        SDL_RenderFillRect(win->renderer, &rect);
    }
}

void graphics_draw_circle(ZenithWindow *win, int cx, int cy, int radius, int r, int g, int b, int a) {
    if (!win || !win->running || !win->renderer) return;
    
    SDL_SetRenderDrawColor(win->renderer, r, g, b, a);
    
    for (int w = 0; w < radius * 2; w++) {
        for (int h = 0; h < radius * 2; h++) {
            int dx = radius - w;
            int dy = radius - h;
            if ((dx*dx + dy*dy) <= (radius * radius)) {
                SDL_RenderDrawPoint(win->renderer, cx + dx, cy + dy);
            }
        }
    }
}

void graphics_clear(ZenithWindow *win, int r, int g, int b) {
    if (!win || !win->running) return;
    
    if (win->use_opengl) {
        glClearColor(r/255.0f, g/255.0f, b/255.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    } else {
        SDL_SetRenderDrawColor(win->renderer, r, g, b, 255);
        SDL_RenderClear(win->renderer);
    }
}

void graphics_present(ZenithWindow *win) {
    if (!win || !win->running) return;
    
    graphics_poll_events(win);
    
    if (win->use_opengl) {
        SDL_GL_SwapWindow(win->window);
    } else {
        SDL_RenderPresent(win->renderer);
    }
    
    SDL_Delay(16);
}
#endif

const char* get_mime_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";
    
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".js") == 0) return "application/javascript";
    if (strcmp(ext, ".json") == 0) return "application/json";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    if (strcmp(ext, ".svg") == 0) return "image/svg+xml";
    if (strcmp(ext, ".ico") == 0) return "image/x-icon";
    if (strcmp(ext, ".txt") == 0) return "text/plain";
    if (strcmp(ext, ".pdf") == 0) return "application/pdf";
    if (strcmp(ext, ".zip") == 0) return "application/zip";
    
    return "application/octet-stream";
}

void *http_server_thread(void *arg) {
    int server_fd, client_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        return NULL;
    }
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(http_server.port);
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        close(server_fd);
        return NULL;
    }
    
    if (listen(server_fd, 10) < 0) {
        close(server_fd);
        return NULL;
    }
    
    printf("🚀 HTTP Server running on http://localhost:%d\n", http_server.port);
    printf("📁 Serving files from: %s\n", http_server.root_dir);
    
    while (http_server.running) {
        if ((client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            continue;
        }
        
        char buffer[8192] = {0};
        read(client_fd, buffer, 8192);
        
        char method[16], path[1024], protocol[16];
        sscanf(buffer, "%s %s %s", method, path, protocol);
        
        if (strcmp(path, "/") == 0) {
            strcpy(path, "/index.html");
        }
        
        char filepath[2048];
        snprintf(filepath, 2048, "%s%s", http_server.root_dir, path);
        
        FILE *file = fopen(filepath, "rb");
        if (file) {
            fseek(file, 0, SEEK_END);
            long fsize = ftell(file);
            fseek(file, 0, SEEK_SET);
            
            char *content = malloc(fsize + 1);
            fread(content, 1, fsize, file);
            fclose(file);
            
            const char *mime = get_mime_type(filepath);
            
            char response[8192];
            int header_len = snprintf(response, 8192,
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: %s\r\n"
                "Content-Length: %ld\r\n"
                "Access-Control-Allow-Origin: *\r\n"
                "Connection: close\r\n\r\n", mime, fsize);
            
            write(client_fd, response, header_len);
            write(client_fd, content, fsize);
            free(content);
        } else {
            char *not_found = 
                "HTTP/1.1 404 Not Found\r\n"
                "Content-Type: text/html\r\n\r\n"
                "<html><body><h1>404 Not Found</h1></body></html>";
            write(client_fd, not_found, strlen(not_found));
        }
        
        close(client_fd);
    }
    
    close(server_fd);
    return NULL;
}

Value *file_read(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        Value *v = create_value(VAL_STRING);
        v->data.string = strdup("");
        return v;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *buffer = malloc(size + 1);
    fread(buffer, 1, size, f);
    buffer[size] = 0;
    fclose(f);
    
    Value *v = create_value(VAL_STRING);
    v->data.string = buffer;
    return v;
}

void file_write(const char *filename, const char *content) {
    FILE *f = fopen(filename, "w");
    if (f) {
        fprintf(f, "%s", content);
        fclose(f);
    }
}

bool file_exists(const char *filename) {
    return access(filename, F_OK) == 0;
}

void file_delete(const char *filename) {
    remove(filename);
}

void file_mkdir(const char *dirname) {
    mkdir(dirname, 0755);
}

Module *load_module(const char *name) {
    for (int i = 0; i < module_count; i++) {
        if (strcmp(modules[i].name, name) == 0) {
            return &modules[i];
        }
    }
    
    char path[512];
    snprintf(path, 512, "%s/%s.zt", MODULE_PATH, name);
    
    if (!file_exists(path)) {
        snprintf(path, 512, "./%s.zt", name);
        if (!file_exists(path)) {
            return NULL;
        }
    }
    
    Module *mod = &modules[module_count++];
    mod->name = strdup_safe(name);
    mod->exports = NULL;
    mod->export_count = 0;
    
    return mod;
}

Token *tokenize(char *line, int *count) {
    Token *tokens = malloc(MAX_TOKENS * sizeof(Token));
    *count = 0;
    char *p = line;
    
    while (*p) {
        while (isspace(*p)) p++;
        if (!*p) break;
        
        Token *tok = &tokens[*count];
        tok->line = 0;
        
        if (*p == '#') {
            while (*p && *p != '\n') p++;
            continue;
        }
        
        if (*p == '"' || *p == '\'' || *p == '`') {
            char quote = *p++;
            char *start = p;
            while (*p && *p != quote) {
                if (*p == '\\' && *(p+1)) p++;
                p++;
            }
            tok->type = TOK_STRING;
            tok->value = strndup(start, p - start);
            if (*p) p++;
            (*count)++;
            continue;
        }
        
        if (isdigit(*p) || (*p == '.' && isdigit(*(p+1)))) {
            char *start = p;
            while (isdigit(*p) || *p == '.') p++;
            if (*p == 'e' || *p == 'E') {
                p++;
                if (*p == '+' || *p == '-') p++;
                while (isdigit(*p)) p++;
            }
            tok->type = TOK_NUMBER;
            tok->value = strndup(start, p - start);
            (*count)++;
            continue;
        }
        
        if (isalpha(*p) || *p == '_') {
            char *start = p;
            while (isalnum(*p) || *p == '_') p++;
            tok->value = strndup(start, p - start);
            
            if (strcmp(tok->value, "print") == 0) tok->type = TOK_PRINT;
            else if (strcmp(tok->value, "input") == 0) tok->type = TOK_INPUT;
            else if (strcmp(tok->value, "if") == 0) tok->type = TOK_IF;
            else if (strcmp(tok->value, "else") == 0) tok->type = TOK_ELSE;
            else if (strcmp(tok->value, "elif") == 0) tok->type = TOK_ELIF;
            else if (strcmp(tok->value, "while") == 0) tok->type = TOK_WHILE;
            else if (strcmp(tok->value, "for") == 0) tok->type = TOK_FOR;
            else if (strcmp(tok->value, "in") == 0) tok->type = TOK_IN;
            else if (strcmp(tok->value, "range") == 0) tok->type = TOK_RANGE;
            else if (strcmp(tok->value, "func") == 0 || strcmp(tok->value, "function") == 0) tok->type = TOK_FUNC;
            else if (strcmp(tok->value, "return") == 0) tok->type = TOK_RETURN;
            else if (strcmp(tok->value, "break") == 0) tok->type = TOK_BREAK;
            else if (strcmp(tok->value, "continue") == 0) tok->type = TOK_CONTINUE;
            else if (strcmp(tok->value, "class") == 0) tok->type = TOK_CLASS;
            else if (strcmp(tok->value, "new") == 0) tok->type = TOK_NEW;
            else if (strcmp(tok->value, "this") == 0) tok->type = TOK_THIS;
            else if (strcmp(tok->value, "static") == 0) tok->type = TOK_STATIC;
            else if (strcmp(tok->value, "let") == 0) tok->type = TOK_LET;
            else if (strcmp(tok->value, "const") == 0) tok->type = TOK_CONST;
            else if (strcmp(tok->value, "var") == 0) tok->type = TOK_VAR;
            else if (strcmp(tok->value, "true") == 0) tok->type = TOK_TRUE;
            else if (strcmp(tok->value, "false") == 0) tok->type = TOK_FALSE;
            else if (strcmp(tok->value, "null") == 0) tok->type = TOK_NULL;
            else if (strcmp(tok->value, "undefined") == 0) tok->type = TOK_UNDEFINED;
            else if (strcmp(tok->value, "import") == 0) tok->type = TOK_IMPORT;
            else if (strcmp(tok->value, "from") == 0) tok->type = TOK_FROM;
            else if (strcmp(tok->value, "as") == 0) tok->type = TOK_AS;
            else if (strcmp(tok->value, "export") == 0) tok->type = TOK_EXPORT;
            else if (strcmp(tok->value, "module") == 0) tok->type = TOK_MODULE;
            else if (strcmp(tok->value, "server") == 0) tok->type = TOK_SERVER;
            else if (strcmp(tok->value, "start") == 0) tok->type = TOK_START;
            else if (strcmp(tok->value, "stop") == 0) tok->type = TOK_STOP;
            else if (strcmp(tok->value, "read") == 0) tok->type = TOK_READ;
            else if (strcmp(tok->value, "write") == 0) tok->type = TOK_WRITE;
            else if (strcmp(tok->value, "delete") == 0) tok->type = TOK_DELETE;
            else if (strcmp(tok->value, "exists") == 0) tok->type = TOK_EXISTS;
            else if (strcmp(tok->value, "mkdir") == 0) tok->type = TOK_MKDIR;
            else if (strcmp(tok->value, "length") == 0) tok->type = TOK_LENGTH;
            else if (strcmp(tok->value, "push") == 0) tok->type = TOK_PUSH;
            else if (strcmp(tok->value, "pop") == 0) tok->type = TOK_POP;
            else if (strcmp(tok->value, "window") == 0) tok->type = TOK_WINDOW;
            else if (strcmp(tok->value, "rect") == 0) tok->type = TOK_RECT;
            else if (strcmp(tok->value, "circle") == 0) tok->type = TOK_CIRCLE;
            else if (strcmp(tok->value, "clear") == 0) tok->type = TOK_LINE;
            else if (strcmp(tok->value, "render") == 0) tok->type = TOK_RENDER;
            else if (strcmp(tok->value, "hash") == 0) tok->type = TOK_HASH;
            else if (strcmp(tok->value, "encrypt") == 0) tok->type = TOK_ENCRYPT;
            else if (strcmp(tok->value, "decrypt") == 0) tok->type = TOK_DECRYPT;
            else if (strcmp(tok->value, "salt") == 0) tok->type = TOK_SALT;
            else if (strcmp(tok->value, "compile") == 0) tok->type = TOK_COMPILE;
            else if (strcmp(tok->value, "tcc") == 0) tok->type = TOK_TCC;
            else if (strcmp(tok->value, "gcc") == 0) tok->type = TOK_GCC;
            else if (strcmp(tok->value, "try") == 0) tok->type = TOK_TRY;
            else if (strcmp(tok->value, "catch") == 0) tok->type = TOK_CATCH;
            else if (strcmp(tok->value, "throw") == 0) tok->type = TOK_THROW;
            else if (strcmp(tok->value, "finally") == 0) tok->type = TOK_FINALLY;
            else if (strcmp(tok->value, "async") == 0) tok->type = TOK_ASYNC;
            else if (strcmp(tok->value, "await") == 0) tok->type = TOK_AWAIT;
            else if (strcmp(tok->value, "typeof") == 0) tok->type = TOK_TYPEOF;
            else if (strcmp(tok->value, "instanceof") == 0) tok->type = TOK_INSTANCEOF;
            else tok->type = TOK_IDENT;
            
            (*count)++;
            continue;
        }
        
        switch (*p) {
            case '(': tok->type = TOK_LPAREN; tok->value = strdup("("); p++; break;
            case ')': tok->type = TOK_RPAREN; tok->value = strdup(")"); p++; break;
            case '{': tok->type = TOK_LBRACE; tok->value = strdup("{"); p++; break;
            case '}': tok->type = TOK_RBRACE; tok->value = strdup("}"); p++; break;
            case '[': tok->type = TOK_LBRACKET; tok->value = strdup("["); p++; break;
            case ']': tok->type = TOK_RBRACKET; tok->value = strdup("]"); p++; break;
            case ',': tok->type = TOK_COMMA; tok->value = strdup(","); p++; break;
            case ':': tok->type = TOK_COLON; tok->value = strdup(":"); p++; break;
            case ';': tok->type = TOK_SEMICOLON; tok->value = strdup(";"); p++; break;
            case '?': tok->type = TOK_QUESTION; tok->value = strdup("?"); p++; break;
            case '@': tok->type = TOK_AT; tok->value = strdup("@"); p++; break;
            case '.': tok->type = TOK_DOT; tok->value = strdup("."); p++; break;
            case '+':
                if (p[1] == '+') {
                    tok->type = TOK_PLUSPLUS; tok->value = strdup("++"); p += 2;
                } else if (p[1] == '=') {
                    tok->type = TOK_PLUSEQ; tok->value = strdup("+="); p += 2;
                } else {
                    tok->type = TOK_PLUS; tok->value = strdup("+"); p++;
                }
                break;
            case '-':
                if (p[1] == '-') {
                    tok->type = TOK_MINUSMINUS; tok->value = strdup("--"); p += 2;
                } else if (p[1] == '>') {
                    tok->type = TOK_ARROW; tok->value = strdup("->"); p += 2;
                } else if (p[1] == '=') {
                    tok->type = TOK_MINUSEQ; tok->value = strdup("-="); p += 2;
                } else {
                    tok->type = TOK_MINUS; tok->value = strdup("-"); p++;
                }
                break;
            case '*':
                if (p[1] == '*') {
                    tok->type = TOK_POWER; tok->value = strdup("**"); p += 2;
                } else if (p[1] == '=') {
                    tok->type = TOK_STAREQ; tok->value = strdup("*="); p += 2;
                } else {
                    tok->type = TOK_STAR; tok->value = strdup("*"); p++;
                }
                break;
            case '/':
                if (p[1] == '=') {
                    tok->type = TOK_SLASHEQ; tok->value = strdup("/="); p += 2;
                } else {
                    tok->type = TOK_SLASH; tok->value = strdup("/"); p++;
                }
                break;
            case '%': tok->type = TOK_PERCENT; tok->value = strdup("%"); p++; break;
            case '=':
                if (p[1] == '=' && p[2] == '=') {
                    tok->type = TOK_EQEQEQ; tok->value = strdup("==="); p += 3;
                } else if (p[1] == '=') {
                    tok->type = TOK_EQEQ; tok->value = strdup("=="); p += 2;
                } else {
                    tok->type = TOK_EQ; tok->value = strdup("="); p++;
                }
                break;
            case '!':
                if (p[1] == '=' && p[2] == '=') {
                    tok->type = TOK_NEQEQ; tok->value = strdup("!=="); p += 3;
                } else if (p[1] == '=') {
                    tok->type = TOK_NEQ; tok->value = strdup("!="); p += 2;
                } else {
                    tok->type = TOK_NOT; tok->value = strdup("!"); p++;
                }
                break;
            case '<':
                if (p[1] == '<') {
                    tok->type = TOK_LSHIFT; tok->value = strdup("<<"); p += 2;
                } else if (p[1] == '=') {
                    tok->type = TOK_LTE; tok->value = strdup("<="); p += 2;
                } else {
                    tok->type = TOK_LT; tok->value = strdup("<"); p++;
                }
                break;
            case '>':
                if (p[1] == '>') {
                    tok->type = TOK_RSHIFT; tok->value = strdup(">>"); p += 2;
                } else if (p[1] == '=') {
                    tok->type = TOK_GTE; tok->value = strdup(">="); p += 2;
                } else {
                    tok->type = TOK_GT; tok->value = strdup(">"); p++;
                }
                break;
            case '&':
                if (p[1] == '&') {
                    tok->type = TOK_AND; tok->value = strdup("&&"); p += 2;
                } else {
                    tok->type = TOK_BITAND; tok->value = strdup("&"); p++;
                }
                break;
            case '|':
                if (p[1] == '|') {
                    tok->type = TOK_OR; tok->value = strdup("||"); p += 2;
                } else {
                    tok->type = TOK_BITOR; tok->value = strdup("|"); p++;
                }
                break;
            case '^': tok->type = TOK_BITXOR; tok->value = strdup("^"); p++; break;
            case '~': tok->type = TOK_BITNOT; tok->value = strdup("~"); p++; break;
            default: p++; break;
  }
        (*count)++;
    }      
    
    return tokens;
}

void free_tokens(Token *tokens, int count) {
    for (int i = 0; i < count; i++) {
        if (tokens[i].value) free(tokens[i].value);
    }
    free(tokens);
}

Value *math_operation(Value *left, Value *right, TokenType op) {
    Value *result = create_value(VAL_NUMBER);
    double l = (left->type == VAL_NUMBER) ? left->data.number : 0;
    double r = (right->type == VAL_NUMBER) ? right->data.number : 0;
    
    switch (op) {
        case TOK_PLUS: result->data.number = l + r; break;
        case TOK_MINUS: result->data.number = l - r; break;
        case TOK_STAR: result->data.number = l * r; break;
        case TOK_SLASH: result->data.number = (r != 0) ? l / r : 0; break;
        case TOK_PERCENT: result->data.number = (r != 0) ? fmod(l, r) : 0; break;
        case TOK_POWER: result->data.number = pow(l, r); break;
        case TOK_BITAND: result->data.number = (long)l & (long)r; break;
        case TOK_BITOR: result->data.number = (long)l | (long)r; break;
        case TOK_BITXOR: result->data.number = (long)l ^ (long)r; break;
        case TOK_LSHIFT: result->data.number = (long)l << (long)r; break;
        case TOK_RSHIFT: result->data.number = (long)l >> (long)r; break;
        default: result->data.number = 0;
    }
    return result;
}

Value *compare_operation(Value *left, Value *right, TokenType op) {
    Value *result = create_value(VAL_BOOL);
    
    if (left->type == VAL_NUMBER && right->type == VAL_NUMBER) {
        double l = left->data.number;
        double r = right->data.number;
        
        switch (op) {
            case TOK_EQEQ:
            case TOK_EQEQEQ: 
                // printf("DEBUG: CMP %g == %g diff %g\n", l, r, fabs(l-r)); fflush(stdout);
                result->data.boolean = (fabs(l - r) < 1e-9); 
                break;
            case TOK_NEQ:
            case TOK_NEQEQ: result->data.boolean = (fabs(l - r) >= 1e-9); break;
            case TOK_LT: result->data.boolean = (l < r); break;
            case TOK_GT: result->data.boolean = (l > r); break;
            case TOK_LTE: result->data.boolean = (l <= r); break;
            case TOK_GTE: result->data.boolean = (l >= r); break;
            default: result->data.boolean = false;
        }
    } else if (left->type == VAL_STRING && right->type == VAL_STRING) {
        int cmp = strcmp(left->data.string, right->data.string);
        switch (op) {
            case TOK_EQEQ:
            case TOK_EQEQEQ: result->data.boolean = (cmp == 0); break;
            case TOK_NEQ:
            case TOK_NEQEQ: result->data.boolean = (cmp != 0); break;
            default: result->data.boolean = false;
        }
    }
    return result;
}

Value *eval_expr(Token *tokens, int tok_count, int *tok_idx) {
    if (*tok_idx >= tok_count) return create_value(VAL_NULL);
    
    Token *tok = &tokens[*tok_idx];
    
    if (tok->type == TOK_NUMBER) {
        (*tok_idx)++;
        Value *v = create_value(VAL_NUMBER);
        v->data.number = atof(tok->value);
        
        if (*tok_idx < tok_count) {
            Token *op = &tokens[*tok_idx];
            if (op->type >= TOK_PLUS && op->type <= TOK_RSHIFT) {
                (*tok_idx)++;
                Value *right = eval_expr(tokens, tok_count, tok_idx);
                Value *result = math_operation(v, right, op->type);
                free_value(v);
                free_value(right);
                return result;
            } else if (op->type >= TOK_EQEQ && op->type <= TOK_GTE) {
                (*tok_idx)++;
                Value *right = eval_expr(tokens, tok_count, tok_idx);
                Value *result = compare_operation(v, right, op->type);
                free_value(v);
                free_value(right);
                return result;
            }
        }
        return v;
    }
    
    if (tok->type == TOK_STRING) {
        (*tok_idx)++;
        Value *v = create_value(VAL_STRING);
        v->data.string = strdup_safe(tok->value);
        
        if (*tok_idx < tok_count && tokens[*tok_idx].type == TOK_PLUS) {
            (*tok_idx)++;
            Value *right = eval_expr(tokens, tok_count, tok_idx);
            char *right_str = value_to_string(right);
            char *result_str = malloc(strlen(v->data.string) + strlen(right_str) + 1);
            sprintf(result_str, "%s%s", v->data.string, right_str);
            free(v->data.string);
            v->data.string = result_str;
            free(right_str);
            free_value(right);
        }
        return v;
    }
    
    if (tok->type == TOK_TRUE) {
        (*tok_idx)++;
        Value *v = create_value(VAL_BOOL);
        v->data.boolean = true;
        return v;
    }
    
    if (tok->type == TOK_FALSE) {
        (*tok_idx)++;
        Value *v = create_value(VAL_BOOL);
        v->data.boolean = false;
        return v;
    }
    
    if (tok->type == TOK_NULL) {
        (*tok_idx)++;
        return create_value(VAL_NULL);
    }
    
    if (tok->type == TOK_UNDEFINED) {
        (*tok_idx)++;
        return create_value(VAL_UNDEFINED);
    }
    
    if (tok->type == TOK_IDENT) {
        char *var_name = tok->value;
        Value *var = get_var(var_name);
        (*tok_idx)++;
        
        // Handle Function Call
        if (*tok_idx < tok_count && tokens[*tok_idx].type == TOK_LPAREN) {
            Function *func = find_function(var_name);
            if (func) {
                (*tok_idx)++; // Skip '('
                 
                 // Create args
                 Value **args = malloc(16 * sizeof(Value*));
                 int arg_count = 0;
                 int arg_cap = 16;
                 
                 while (*tok_idx < tok_count && tokens[*tok_idx].type != TOK_RPAREN) {
                     if (arg_count >= arg_cap) {
                         arg_cap *= 2;
                         args = realloc(args, arg_cap * sizeof(Value*));
                     }
                     args[arg_count++] = eval_expr(tokens, tok_count, tok_idx);
                     if (*tok_idx < tok_count && tokens[*tok_idx].type == TOK_COMMA) (*tok_idx)++;
                 }
                 if (*tok_idx < tok_count && tokens[*tok_idx].type == TOK_RPAREN) (*tok_idx)++;
                 
                 // Save state
                 Value *old_ret = return_val;
                 bool old_is_ret = is_returning;
                 int old_scope = current_scope;
                 int old_var_count = var_count;
                 
                 // Setup new call
                 return_val = NULL;
                 is_returning = false;
                 current_scope++; // Simple scope increment
                 
                 // Bind params
                 for (int i = 0; i < func->param_count; i++) {
                     if (i < arg_count) {
                         set_var(func->params[i], args[i], false);
                     }
                 }
                 
                 // Execute body
                 int f_idx = 0;
                 execute_block(func->body_tokens, func->body_token_count, &f_idx);
                 
                 // Get result
                 Value *ret = return_val ? return_val : create_value(VAL_NULL);
                 
                 // Cleanup args
                 for(int i=0; i<arg_count; i++) free_value(args[i]);
                 free(args);

                 // Pop stack vars
                 while (var_count > old_var_count) {
                    var_count--;
                    free(vars[var_count].name);
                    free_value(vars[var_count].value);
                 }
                 
                 // Restore state
                 return_val = old_ret;
                 is_returning = old_is_ret;
                 current_scope = old_scope;
                 
                 return ret;
            }
        }
        
        if (*tok_idx < tok_count && tokens[*tok_idx].type == TOK_PLUSPLUS) {
            (*tok_idx)++;
            if (var && var->type == VAL_NUMBER) {
                Value *result = create_value(VAL_NUMBER);
                result->data.number = var->data.number;
                var->data.number += 1;
                return result;
            }
        }
        
        if (*tok_idx < tok_count && tokens[*tok_idx].type == TOK_MINUSMINUS) {
            (*tok_idx)++;
            if (var && var->type == VAL_NUMBER) {
                Value *result = create_value(VAL_NUMBER);
                result->data.number = var->data.number;
                var->data.number -= 1;
                return result;
            }
        }
        
        if (var) {
            if (*tok_idx < tok_count) {
                Token *op = &tokens[*tok_idx];
                
                if (op->type >= TOK_PLUS && op->type <= TOK_RSHIFT) {
                    (*tok_idx)++;
                    Value *right = eval_expr(tokens, tok_count, tok_idx);
                    Value *result = math_operation(var, right, op->type);
                    free_value(right);
                    return result;
                }
                
                if (op->type >= TOK_EQEQ && op->type <= TOK_GTE) {
                    (*tok_idx)++;
                    Value *right = eval_expr(tokens, tok_count, tok_idx);
                    Value *result = compare_operation(var, right, op->type);
                    free_value(right);
                    return result;
                }
                
                if (op->type == TOK_LBRACKET) {
                    (*tok_idx)++;
                    Value *index = eval_expr(tokens, tok_count, tok_idx);
                    if (*tok_idx < tok_count && tokens[*tok_idx].type == TOK_RBRACKET) {
                        (*tok_idx)++;
                    }
                    
                    if (var->type == VAL_ARRAY && index->type == VAL_NUMBER) {
                        int idx = (int)index->data.number;
                        if (idx >= 0 && idx < var->data.array.count) {
                            Value *result = copy_value(var->data.array.items[idx]);
                            free_value(index);
                            return result;
                        }
                    }
                    free_value(index);
                }
            }
            return copy_value(var);
        }
        return create_value(VAL_NULL);
    }
    
    if (tok->type == TOK_INPUT) {
        (*tok_idx)++;
        if (*tok_idx < tok_count && tokens[*tok_idx].type == TOK_LPAREN) {
            (*tok_idx)++;
            char *prompt = NULL;
            if (*tok_idx < tok_count && tokens[*tok_idx].type == TOK_STRING) {
                prompt = tokens[*tok_idx].value;
                (*tok_idx)++;
            }
            if (*tok_idx < tok_count && tokens[*tok_idx].type == TOK_RPAREN) {
                (*tok_idx)++;
            }
            
            if (prompt) printf("%s", prompt);
            fflush(stdout);
            char buffer[MAX_LINE];
            if (fgets(buffer, MAX_LINE, stdin)) {
                buffer[strcspn(buffer, "\n")] = 0;
                Value *v = create_value(VAL_STRING);
                v->data.string = strdup_safe(buffer);
                return v;
            }
        }
        return create_value(VAL_STRING);
    }
    
    if (tok->type == TOK_LBRACKET) {
        (*tok_idx)++;
        Value *arr = create_value(VAL_ARRAY);
        
        while (*tok_idx < tok_count && tokens[*tok_idx].type != TOK_RBRACKET) {
            Value *item = eval_expr(tokens, tok_count, tok_idx);
            
            if (arr->data.array.count >= arr->data.array.capacity) {
                arr->data.array.capacity *= 2;
                arr->data.array.items = realloc(arr->data.array.items, 
                    arr->data.array.capacity * sizeof(Value*));
            }
            arr->data.array.items[arr->data.array.count++] = item;
            
            if (*tok_idx < tok_count && tokens[*tok_idx].type == TOK_COMMA) {
                (*tok_idx)++;
            }
        }
        
        if (*tok_idx < tok_count && tokens[*tok_idx].type == TOK_RBRACKET) {
            (*tok_idx)++;
        }
        return arr;
    }
    
    if (tok->type == TOK_RANGE) {
        (*tok_idx)++;
        if (*tok_idx < tok_count && tokens[*tok_idx].type == TOK_LPAREN) {
            (*tok_idx)++;
            Value *start_val = NULL, *end_val = NULL, *step_val = NULL;
            
            end_val = eval_expr(tokens, tok_count, tok_idx);
            
            if (*tok_idx < tok_count && tokens[*tok_idx].type == TOK_COMMA) {
                (*tok_idx)++;
                start_val = end_val;
                end_val = eval_expr(tokens, tok_count, tok_idx);
                
                if (*tok_idx < tok_count && tokens[*tok_idx].type == TOK_COMMA) {
                    (*tok_idx)++;
                    step_val = eval_expr(tokens, tok_count, tok_idx);
                }
            }
            
            if (*tok_idx < tok_count && tokens[*tok_idx].type == TOK_RPAREN) {
                (*tok_idx)++;
            }
            
            int start = start_val ? (int)start_val->data.number : 0;
            int end = (int)end_val->data.number;
            int step = step_val ? (int)step_val->data.number : 1;
            
            Value *arr = create_value(VAL_ARRAY);
            for (int i = start; step > 0 ? i < end : i > end; i += step) {
                Value *item = create_value(VAL_NUMBER);
                item->data.number = i;
                
                if (arr->data.array.count >= arr->data.array.capacity) {
                    arr->data.array.capacity *= 2;
                    arr->data.array.items = realloc(arr->data.array.items,
                        arr->data.array.capacity * sizeof(Value*));
                }
                arr->data.array.items[arr->data.array.count++] = item;
            }
            
            if (start_val) free_value(start_val);
            free_value(end_val);
            if (step_val) free_value(step_val);
            return arr;
        }
    }
    
    if (tok->type == TOK_LENGTH) {
        (*tok_idx)++;
        if (*tok_idx < tok_count && tokens[*tok_idx].type == TOK_LPAREN) {
            (*tok_idx)++;
            Value *val = eval_expr(tokens, tok_count, tok_idx);
            if (*tok_idx < tok_count && tokens[*tok_idx].type == TOK_RPAREN) {
                (*tok_idx)++;
            }
            
            Value *len = create_value(VAL_NUMBER);
            if (val->type == VAL_ARRAY) {
                len->data.number = val->data.array.count;
            } else if (val->type == VAL_STRING) {
                len->data.number = strlen(val->data.string);
            }
            free_value(val);
            return len;
        }
    }
    
    if (tok->type == TOK_READ) {
        (*tok_idx)++;
        if (*tok_idx < tok_count && tokens[*tok_idx].type == TOK_LPAREN) {
            (*tok_idx)++;
            Value *filename = eval_expr(tokens, tok_count, tok_idx);
            if (*tok_idx < tok_count && tokens[*tok_idx].type == TOK_RPAREN) {
                (*tok_idx)++;
            }
            
            char *fname = value_to_string(filename);
            Value *content = file_read(fname);
            free(fname);
            free_value(filename);
            return content;
        }
    }
    
    if (tok->type == TOK_EXISTS) {
        (*tok_idx)++;
        if (*tok_idx < tok_count && tokens[*tok_idx].type == TOK_LPAREN) {
            (*tok_idx)++;
            Value *filename = eval_expr(tokens, tok_count, tok_idx);
            if (*tok_idx < tok_count && tokens[*tok_idx].type == TOK_RPAREN) {
                (*tok_idx)++;
            }
            
            char *fname = value_to_string(filename);
            bool exists = file_exists(fname);
            free(fname);
            free_value(filename);
            
            Value *v = create_value(VAL_BOOL);
            v->data.boolean = exists;
            return v;
        }
    }
    
    if (tok->type == TOK_HASH) {
        (*tok_idx)++;
        if (*tok_idx < tok_count && tokens[*tok_idx].type == TOK_LPAREN) {
            (*tok_idx)++;
            Value *data = eval_expr(tokens, tok_count, tok_idx);
            
            Value *algorithm = NULL;
            if (*tok_idx < tok_count && tokens[*tok_idx].type == TOK_COMMA) {
                (*tok_idx)++;
                algorithm = eval_expr(tokens, tok_count, tok_idx);
            }
            
            if (*tok_idx < tok_count && tokens[*tok_idx].type == TOK_RPAREN) {
                (*tok_idx)++;
            }
            
            char *data_str = value_to_string(data);
            char *algo_str = algorithm ? value_to_string(algorithm) : strdup("sha256");
            Value *hash = crypto_hash(data_str, algo_str);
            
            free(data_str);
            free(algo_str);
            free_value(data);
            if (algorithm) free_value(algorithm);
            return hash;
        }
    }
    
    if (tok->type == TOK_ENCRYPT) {
        (*tok_idx)++;
        if (*tok_idx < tok_count && tokens[*tok_idx].type == TOK_LPAREN) {
            (*tok_idx)++;
            Value *data = eval_expr(tokens, tok_count, tok_idx);
            if (*tok_idx < tok_count && tokens[*tok_idx].type == TOK_COMMA) (*tok_idx)++;
            Value *key = eval_expr(tokens, tok_count, tok_idx);
            if (*tok_idx < tok_count && tokens[*tok_idx].type == TOK_RPAREN) (*tok_idx)++;
            
            char *data_str = value_to_string(data);
            char *key_str = value_to_string(key);
            Value *encrypted = crypto_encrypt_aes(data_str, key_str);
            free(data_str); free(key_str);
            free_value(data); free_value(key);
            return encrypted;
        }
    }
    
    if (tok->type == TOK_DECRYPT) {
        (*tok_idx)++;
        if (*tok_idx < tok_count && tokens[*tok_idx].type == TOK_LPAREN) {
            (*tok_idx)++;
            Value *data = eval_expr(tokens, tok_count, tok_idx);
            if (*tok_idx < tok_count && tokens[*tok_idx].type == TOK_COMMA) (*tok_idx)++;
            Value *key = eval_expr(tokens, tok_count, tok_idx);
            if (*tok_idx < tok_count && tokens[*tok_idx].type == TOK_RPAREN) (*tok_idx)++;
            
            char *data_str = value_to_string(data);
            char *key_str = value_to_string(key);
            Value *decrypted = crypto_decrypt_aes(data_str, key_str);
            free(data_str); free(key_str);
            free_value(data); free_value(key);
            return decrypted;
        }
    }
    
    if (tok->type == TOK_SALT) {
        (*tok_idx)++;
        if (*tok_idx < tok_count && tokens[*tok_idx].type == TOK_LPAREN) {
            (*tok_idx)++;
            Value *length = eval_expr(tokens, tok_count, tok_idx);
            if (*tok_idx < tok_count && tokens[*tok_idx].type == TOK_RPAREN) (*tok_idx)++;
            int len = (int)length->data.number;
            Value *salt = crypto_generate_salt(len > 0 ? len : 32);
            free_value(length);
            return salt;
        }
    }
    
#ifdef HAVE_SDL2
    if (tok->type == TOK_WINDOW) {
        (*tok_idx)++;
        if (*tok_idx < tok_count && tokens[*tok_idx].type == TOK_LPAREN) {
            (*tok_idx)++;
            Value *title = eval_expr(tokens, tok_count, tok_idx);
            if (*tok_idx < tok_count && tokens[*tok_idx].type == TOK_COMMA) (*tok_idx)++;
            Value *width = eval_expr(tokens, tok_count, tok_idx);
            if (*tok_idx < tok_count && tokens[*tok_idx].type == TOK_COMMA) (*tok_idx)++;
            Value *height = eval_expr(tokens, tok_count, tok_idx);
            
            bool use_gl = false;
            if (*tok_idx < tok_count && tokens[*tok_idx].type == TOK_COMMA) {
                (*tok_idx)++;
                Value *gl_val = eval_expr(tokens, tok_count, tok_idx);
                use_gl = gl_val->data.boolean;
                free_value(gl_val);
            }
            
            if (*tok_idx < tok_count && tokens[*tok_idx].type == TOK_RPAREN) (*tok_idx)++;
            
            char *title_str = value_to_string(title);
            int w = (int)width->data.number;
            int h = (int)height->data.number;
            
            Value *win = graphics_create_window(title_str, w, h, use_gl);
            free(title_str);
            free_value(title); free_value(width); free_value(height);
            return win;
        }
    }
#endif
    
    return create_value(VAL_NULL);
}

// Helper to skip a block of code {...}
void skip_block(Token *tokens, int count, int *idx) {
    if (*idx >= count) return;
    if (tokens[*idx].type != TOK_LBRACE) return;
    
    int braces = 1;
    (*idx)++;
    
    while (*idx < count && braces > 0) {
        if (tokens[*idx].type == TOK_LBRACE) braces++;
        else if (tokens[*idx].type == TOK_RBRACE) braces--;
        (*idx)++;
    }
}

Function *find_function(const char *name) {
    for (int i = 0; i < func_count; i++) {
        if (strcmp(funcs[i].name, name) == 0) {
            return &funcs[i];
        }
    }
    return NULL;
}

// Executes a block of code {...}
void execute_block(Token *tokens, int count, int *idx) {
    if (*idx >= count || tokens[*idx].type != TOK_LBRACE) return;
    (*idx)++; // Skip '{'
    
    while (*idx < count && tokens[*idx].type != TOK_RBRACE) {
        if (is_returning) break;
        execute_statement(tokens, count, idx);  
    }
    
    if (*idx < count && tokens[*idx].type == TOK_RBRACE) {
        (*idx)++; // Skip '}'
    }
}

// Refactored execute_statement that advances the index
// Refactored execute_statement that advances the index
void execute_statement(Token *tokens, int count, int *idx) {
    if (*idx >= count || is_returning) return;
    
    Token *tok = &tokens[*idx];
    
    // Function Definition
    if (tok->type == TOK_FUNC) {
        (*idx)++;
        if (*idx < count && tokens[*idx].type == TOK_IDENT) {
            char *name = tokens[*idx].value;
            (*idx)++;
            
            Function func = {0};
            func.name = strdup_safe(name);
            func.params = calloc(16, sizeof(char*));
            func.param_count = 0;
            
            if (*idx < count && tokens[*idx].type == TOK_LPAREN) {
                (*idx)++;
                while (*idx < count && tokens[*idx].type != TOK_RPAREN) {
                    if (tokens[*idx].type == TOK_IDENT) {
                        func.params[func.param_count++] = strdup_safe(tokens[*idx].value);
                    }
                    (*idx)++;
                    if (*idx < count && tokens[*idx].type == TOK_COMMA) (*idx)++;
                }
                if (*idx < count && tokens[*idx].type == TOK_RPAREN) (*idx)++;
            }
            
            if (*idx < count && tokens[*idx].type == TOK_LBRACE) {
                int start_idx = *idx;
                skip_block(tokens, count, idx);
                int end_idx = *idx; 
                
                // Copy tokens
                int body_len = end_idx - start_idx;
                func.body_tokens = malloc(body_len * sizeof(Token));
                func.body_token_count = body_len;
                // Need deep copy of values? Yes.
                for (int i = 0; i < body_len; i++) {
                    func.body_tokens[i] = tokens[start_idx + i];
                    func.body_tokens[i].value = strdup_safe(tokens[start_idx + i].value);
                }
            }
            
            if (func_count < MAX_FUNCTIONS) {
                funcs[func_count++] = func;
            }
        }
        return;
    }
    
    // Return Statement
    if (tok->type == TOK_RETURN) {
        (*idx)++;
        if (*idx < count && tokens[*idx].type != TOK_SEMICOLON) {
            return_val = eval_expr(tokens, count, idx);
        } else {
            return_val = create_value(VAL_NULL);
        }
        is_returning = true;
        if (*idx < count && tokens[*idx].type == TOK_SEMICOLON) (*idx)++;
        return;
    }
    
    if (tok->type == TOK_IF) {
        (*idx)++;
        if (*idx < count && tokens[*idx].type == TOK_LPAREN) (*idx)++; // Optional parens
        
        Value *cond = eval_expr(tokens, count, idx);
        
        if (*idx < count && tokens[*idx].type == TOK_RPAREN) (*idx)++;
        
        if (cond->data.boolean) {
            execute_block(tokens, count, idx);
            // Handle Else/Elif skip
            while (*idx < count && (tokens[*idx].type == TOK_ELSE || tokens[*idx].type == TOK_ELIF)) {
                 if (tokens[*idx].type == TOK_ELSE) {
                     (*idx)++;
                     skip_block(tokens, count, idx);
                     break;
                 }
                 // For elif, we'd need to skip the condition too, which is harder without parsing.
                 // For now, simplify: assume standard if/else structure.
                 if (tokens[*idx].type == TOK_ELIF) {
                      // Skip ELIF + condition + block
                      (*idx)++;
                       // Skip condition (hacky: skip until {)
                      while (*idx < count && tokens[*idx].type != TOK_LBRACE) (*idx)++;
                      skip_block(tokens, count, idx);
                 }
            }
        } else {
            // Condition false, skip block
            skip_block(tokens, count, idx);
            
            // Check for Else/Elif
            if (*idx < count) {
                if (tokens[*idx].type == TOK_ELSE) {
                    (*idx)++;
                    execute_block(tokens, count, idx);
                } else if (tokens[*idx].type == TOK_ELIF) {
                    // Recursive IF logic... basically treat as new IF
                    // Simple hack: change ELIF to IF and don't advance idx, let next loop handle it?
                    // No, we are in a single pass.
                    // Recursive call to execute_statement_ptr? 
                    tokens[*idx].type = TOK_IF; // Hack: Transform ELIF to IF for recursion
                    execute_statement(tokens, count, idx);
                }
            }
        }
        free_value(cond);
        return;
    }
    
    if (tok->type == TOK_WHILE) {
        (*idx)++;
        int cond_start_idx = *idx;
        
        while (1) {
            int temp_idx = cond_start_idx;
            if (tokens[temp_idx].type == TOK_LPAREN) temp_idx++;
            
            Value *cond = eval_expr(tokens, count, &temp_idx);
            
            bool loop = cond->data.boolean;
            free_value(cond);
            
            if (!loop) {
                // Skip block and exit
                // need to find where the block starts from cond_start_idx to skip it
                int block_start = temp_idx;
                if (tokens[block_start].type == TOK_RPAREN) block_start++;
                skip_block(tokens, count, &block_start);
                *idx = block_start;
                break;
            }
            
            // Execute block
            int block_idx = temp_idx;
            if (tokens[block_idx].type == TOK_RPAREN) block_idx++;
            
            execute_block(tokens, count, &block_idx);
            // Loop back, don't update *idx yet, we go back to cond_start_idx
        }
        return;
    }

    if (tok->type == TOK_LET || tok->type == TOK_CONST || tok->type == TOK_VAR) {
        bool is_const = (tok->type == TOK_CONST);
        (*idx)++;
        
        if ((*idx) < count && tokens[(*idx)].type == TOK_IDENT) {
            char *var_name = tokens[(*idx)].value;
            (*idx)++;
            
            if ((*idx) < count && tokens[(*idx)].type == TOK_EQ) {
                (*idx)++;
                Value *value = eval_expr(tokens, count, idx);
                set_var(var_name, value, is_const);
                free_value(value);
            }
        }
    }
    else if (tok->type == TOK_PRINT) {
        (*idx)++;
        if ((*idx) < count && tokens[(*idx)].type == TOK_LPAREN) {
            (*idx)++;
            while ((*idx) < count && tokens[(*idx)].type != TOK_RPAREN) {
                Value *val = eval_expr(tokens, count, idx);
                char *str = value_to_string(val);
                printf("%s", str);
                free(str);
                free_value(val);
                if ((*idx) < count && tokens[(*idx)].type == TOK_COMMA) {
                    (*idx)++;
                    printf(" ");
                }
            }
            printf("\n");
            if (repl_mode) fflush(stdout);
            else fflush(stdout); // Always flush for now to avoid buffering issues
            
            if ((*idx) < count && tokens[(*idx)].type == TOK_RPAREN) (*idx)++;
        }
    }
    else if (tok->type == TOK_IDENT) {
        bool is_assignment = false;
        if ((*idx) + 1 < count) {
            TokenType next_type = tokens[(*idx) + 1].type;
            if (next_type == TOK_EQ || next_type == TOK_PLUSEQ || 
                next_type == TOK_MINUSEQ || next_type == TOK_STAREQ || 
                next_type == TOK_SLASHEQ) {
                is_assignment = true;
            }
        }

        if (is_assignment) {
            char *var_name = tok->value;
            (*idx)++;
            
            // ... (existing simple assignment code) ...
            if ((*idx) < count) {
                 TokenType op_type = tokens[(*idx)].type;
                 if (op_type == TOK_EQ) {
                    (*idx)++;
                    Value *value = eval_expr(tokens, count, idx);
                    set_var(var_name, value, false);
                    free_value(value);
                 }
                 // ... handles += etc ...
            }
        } else if ((*idx) + 1 < count && tokens[(*idx) + 1].type == TOK_LBRACKET) {
            // Array/Dict assignment: var[idx] = val
            char *var_name = tok->value;
            (*idx)++; // Skip name
            (*idx)++; // Skip [
            Value *index = eval_expr(tokens, count, idx);
            if ((*idx) < count && tokens[(*idx)].type == TOK_RBRACKET) (*idx)++;
            
            if ((*idx) < count && tokens[(*idx)].type == TOK_EQ) {
                (*idx)++;
                Value *val = eval_expr(tokens, count, idx);
                
                Value *var = get_var(var_name);
                if (var) {
                    if (var->type == VAL_ARRAY && index->type == VAL_NUMBER) {
                        int i = (int)index->data.number;
                        if (i >= 0 && i < var->data.array.count) {
                            free_value(var->data.array.items[i]);
                            var->data.array.items[i] = copy_value(val);
                        }
                    } else if (var->type == VAL_DICT && index->type == VAL_STRING) {
                        // Implement dict set
                    }
                }
                free_value(val);
            }
            free_value(index);
        } else {
            // Expression statement
            // Expression statement (e.g. function call)
            Value *v = eval_expr(tokens, count, idx);
            free_value(v);
        }
    }
    else if (tok->type == TOK_START) {
        (*idx)++;
        if ((*idx) < count && tokens[(*idx)].type == TOK_HTTP) {
            (*idx)++;
            if ((*idx) < count && tokens[(*idx)].type == TOK_MINUS) {
                (*idx)++;
                if ((*idx) < count && tokens[(*idx)].type == TOK_SERVER) {
                    (*idx)++;
                    
                    int port = 8000;
                    char *root = ".";
                    
                    while ((*idx) < count) {
                        if (tokens[(*idx)].type == TOK_IDENT) {
                            char *arg = tokens[(*idx)].value;
                            if (strncmp(arg, "port=", 5) == 0) {
                                port = atoi(arg + 5);
                            } else if (strncmp(arg, "root=", 5) == 0) {
                                root = arg + 5;
                            }
                        }
                        (*idx)++;
                    }
                    
                    http_server.port = port;
                    http_server.root_dir = strdup(root);
                    http_server.running = true;
                    pthread_create(&http_server.thread, NULL, http_server_thread, NULL);
                }
            }
        } else if ((*idx) < count && tokens[(*idx)].type == TOK_SERVER) {
            (*idx)++;
            if ((*idx) < count && tokens[(*idx)].type == TOK_LPAREN) {
                (*idx)++;
                Value *port = eval_expr(tokens, count, idx);
                if ((*idx) < count && tokens[(*idx)].type == TOK_RPAREN) (*idx)++;
                
                http_server.port = (int)port->data.number;
                http_server.root_dir = strdup(".");
                http_server.running = true;
                pthread_create(&http_server.thread, NULL, http_server_thread, NULL);
                free_value(port);
            }
        }
    }
    else if (tok->type == TOK_STOP) {
        (*idx)++;
        if ((*idx) < count && tokens[(*idx)].type == TOK_SERVER) {
            http_server.running = false;
            pthread_join(http_server.thread, NULL);
            printf("Server stopped\n");
        }
    }
    else if (tok->type == TOK_WRITE) {
        (*idx)++;
        if ((*idx) < count && tokens[(*idx)].type == TOK_LPAREN) {
            (*idx)++;
            Value *filename = eval_expr(tokens, count, idx);
            if ((*idx) < count && tokens[(*idx)].type == TOK_COMMA) (*idx)++;
            Value *content = eval_expr(tokens, count, idx);
            if ((*idx) < count && tokens[(*idx)].type == TOK_RPAREN) (*idx)++;
            
            char *fname = value_to_string(filename);
            char *cont = value_to_string(content);
            file_write(fname, cont);
            free(fname); free(cont);
            free_value(filename); free_value(content);
        }
    }
    else if (tok->type == TOK_DELETE) {
        (*idx)++;
        if ((*idx) < count && tokens[(*idx)].type == TOK_LPAREN) {
            (*idx)++;
            Value *filename = eval_expr(tokens, count, idx);
            if ((*idx) < count && tokens[(*idx)].type == TOK_RPAREN) (*idx)++;
            
            char *fname = value_to_string(filename);
            file_delete(fname);
            free(fname);
            free_value(filename);
        }
    }
    else if (tok->type == TOK_MKDIR) {
        (*idx)++;
        if ((*idx) < count && tokens[(*idx)].type == TOK_LPAREN) {
            (*idx)++;
            Value *dirname = eval_expr(tokens, count, idx);
            if ((*idx) < count && tokens[(*idx)].type == TOK_RPAREN) (*idx)++;
            
            char *dname = value_to_string(dirname);
            file_mkdir(dname);
            free(dname);
            free_value(dirname);
        }
    }
    else if (tok->type == TOK_IMPORT) {
        (*idx)++;
        if ((*idx) < count && tokens[(*idx)].type == TOK_IDENT) {
            char *module_name = tokens[(*idx)].value;
            (*idx)++;
            
            Module *mod = load_module(module_name);
            if (mod) {
                Value *v = create_value(VAL_MODULE);
                v->data.module = mod;
                set_var(module_name, v, true);
                free_value(v);
            } else {
                printf("Error: Module '%s' not found\n", module_name);
            }
        }
    }
#ifdef HAVE_SDL2
    else if (tok->type == TOK_LINE) {
        (*idx)++;
        if ((*idx) < count && tokens[(*idx)].type == TOK_LPAREN) {
            (*idx)++;
            Value *win_val = eval_expr(tokens, count, idx);
            if ((*idx) < count && tokens[(*idx)].type == TOK_COMMA) (*idx)++;
            Value *r = eval_expr(tokens, count, idx);
            if ((*idx) < count && tokens[(*idx)].type == TOK_COMMA) (*idx)++;
            Value *g = eval_expr(tokens, count, idx);
            if ((*idx) < count && tokens[(*idx)].type == TOK_COMMA) (*idx)++;
            Value *b = eval_expr(tokens, count, idx);
            if ((*idx) < count && tokens[(*idx)].type == TOK_RPAREN) (*idx)++;
            
            if (win_val->type == VAL_WINDOW) {
                graphics_clear(win_val->data.window, (int)r->data.number, 
                             (int)g->data.number, (int)b->data.number);
            }
            free_value(win_val); free_value(r); free_value(g); free_value(b);
        }
    }
    else if (tok->type == TOK_RECT) {
        (*idx)++;
        if ((*idx) < count && tokens[(*idx)].type == TOK_LPAREN) {
            (*idx)++;
            Value *win_val = eval_expr(tokens, count, idx);
            if ((*idx) < count && tokens[(*idx)].type == TOK_COMMA) (*idx)++;
            Value *x = eval_expr(tokens, count, idx);
            if ((*idx) < count && tokens[(*idx)].type == TOK_COMMA) (*idx)++;
            Value *y = eval_expr(tokens, count, idx);
            if ((*idx) < count && tokens[(*idx)].type == TOK_COMMA) (*idx)++;
            Value *w = eval_expr(tokens, count, idx);
            if ((*idx) < count && tokens[(*idx)].type == TOK_COMMA) (*idx)++;
            Value *h = eval_expr(tokens, count, idx);
            if ((*idx) < count && tokens[(*idx)].type == TOK_COMMA) (*idx)++;
            Value *r = eval_expr(tokens, count, idx);
            if ((*idx) < count && tokens[(*idx)].type == TOK_COMMA) (*idx)++;
            Value *g = eval_expr(tokens, count, idx);
            if ((*idx) < count && tokens[(*idx)].type == TOK_COMMA) (*idx)++;
            Value *b = eval_expr(tokens, count, idx);
            if ((*idx) < count && tokens[(*idx)].type == TOK_COMMA) (*idx)++;
            Value *a = eval_expr(tokens, count, idx);
            if ((*idx) < count && tokens[(*idx)].type == TOK_RPAREN) (*idx)++;
            
            if (win_val->type == VAL_WINDOW) {
                graphics_draw_rect(win_val->data.window, (int)x->data.number, (int)y->data.number,
                                 (int)w->data.number, (int)h->data.number, (int)r->data.number,
                                 (int)g->data.number, (int)b->data.number, (int)a->data.number);
            }
            free_value(win_val); free_value(x); free_value(y); free_value(w); free_value(h);
            free_value(r); free_value(g); free_value(b); free_value(a);
        }
    }
    else if (tok->type == TOK_CIRCLE) {
        (*idx)++;
        if ((*idx) < count && tokens[(*idx)].type == TOK_LPAREN) {
            (*idx)++;
            Value *win_val = eval_expr(tokens, count, idx);
            if ((*idx) < count && tokens[(*idx)].type == TOK_COMMA) (*idx)++;
            Value *cx = eval_expr(tokens, count, idx);
            if ((*idx) < count && tokens[(*idx)].type == TOK_COMMA) (*idx)++;
            Value *cy = eval_expr(tokens, count, idx);
            if ((*idx) < count && tokens[(*idx)].type == TOK_COMMA) (*idx)++;
            Value *rad = eval_expr(tokens, count, idx);
            if ((*idx) < count && tokens[(*idx)].type == TOK_COMMA) (*idx)++;
            Value *r = eval_expr(tokens, count, idx);
            if ((*idx) < count && tokens[(*idx)].type == TOK_COMMA) (*idx)++;
            Value *g = eval_expr(tokens, count, idx);
            if ((*idx) < count && tokens[(*idx)].type == TOK_COMMA) (*idx)++;
            Value *b = eval_expr(tokens, count, idx);
            if ((*idx) < count && tokens[(*idx)].type == TOK_COMMA) (*idx)++;
            Value *a = eval_expr(tokens, count, idx);
            if ((*idx) < count && tokens[(*idx)].type == TOK_RPAREN) (*idx)++;
            
            if (win_val->type == VAL_WINDOW) {
                graphics_draw_circle(win_val->data.window, (int)cx->data.number, (int)cy->data.number,
                                   (int)rad->data.number, (int)r->data.number, (int)g->data.number,
                                   (int)b->data.number, (int)a->data.number);
            }
            free_value(win_val); free_value(cx); free_value(cy); free_value(rad);
            free_value(r); free_value(g); free_value(b); free_value(a);
        }
    }
    else if (tok->type == TOK_RENDER) {
        (*idx)++;
        if ((*idx) < count && tokens[(*idx)].type == TOK_LPAREN) {
            (*idx)++;
            Value *win_val = eval_expr(tokens, count, idx);
            if ((*idx) < count && tokens[(*idx)].type == TOK_RPAREN) (*idx)++;
            
            if (win_val->type == VAL_WINDOW) {
                graphics_present(win_val->data.window);
            }
            free_value(win_val);
        }
    }
#endif
    else {
        // Fallback: Expression Statement
        // For any other token, try to evaluate it as an expression and discard result
        Value *v = eval_expr(tokens, count, idx);
        free_value(v);
    }
}

void execute_statement(Token *tokens, int count, int *idx);

void execute_file(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        printf("Error: Cannot open file '%s'\n", filename);
        return;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *buffer = malloc(size + 1);
    fread(buffer, 1, size, f);
    buffer[size] = 0;
    fclose(f);
    
    int count;
    Token *tokens = tokenize(buffer, &count);
    free(buffer);
    
    int idx = 0;
    while (idx < count) {
        execute_statement(tokens, count, &idx);
    }
    
    free_tokens(tokens, count);
}


void print_version() {
    printf("╔═══════════════════════════════════════════╗\n");
    printf("║   Zenith Language v%s             ║\n", VERSION);
    printf("║   Production Ready • Feature Complete    ║\n");
    printf("╚═══════════════════════════════════════════╝\n");
}

void print_help() {
    printf("Usage: zenith [command] [options] [file.zt]\n\n");
    printf("Commands:\n");
    printf("  start http-server [options]  Start HTTP file server\n");
    printf("  stop server                   Stop running server\n");
    printf("  compile [options]             Compile to binary\n\n");
    printf("Options:\n");
    printf("  -v, --version                 Show version\n");
    printf("  -h, --help                    Show help\n");
    printf("  port=<num>                    Server port (default: 8000)\n");
    printf("  root=<dir>                    Server root directory (default: .)\n");
    printf("  --tcc                         Use TCC compiler\n");
    printf("  --gcc                         Use GCC compiler\n");
    printf("  -o <file>                     Output file\n\n");
    printf("Examples:\n");
    printf("  zenith app.zt                          # Run script\n");
    printf("  zenith start http-server port=5000     # Start server on port 5000\n");
    printf("  zenith start http-server root=./public # Serve from ./public\n");
    printf("  zenith compile app.zt --tcc -o myapp   # Compile with TCC\n");
    printf("  zenith                                 # Start REPL\n");
}

void escape_string_for_c(const char *input, char *output, size_t max_len) {
    size_t j = 0;
    for (size_t i = 0; input[i] && j < max_len - 2; i++) {
        if (input[i] == '"') {
            output[j++] = '\\'; output[j++] = '"';
        } else if (input[i] == '\\') {
            output[j++] = '\\'; output[j++] = '\\';
        } else if (input[i] == '\n') {
            output[j++] = '\\'; output[j++] = 'n';
        } else {
            output[j++] = input[i];
        }
    }
    output[j] = 0;
}

#ifndef ZENITH_NO_MAIN
int main(int argc, char *argv[]) {
    if (argc > 1) {
        if (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0) {
            print_version();
            return 0;
        }
        if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
            print_help();
            return 0;
        }
        if (strcmp(argv[1], "start") == 0) {
            char cmd_line[MAX_LINE] = "start ";
            for (int i = 2; i < argc; i++) {
                strcat(cmd_line, argv[i]);
                strcat(cmd_line, " ");
            }
            int count;
            Token *tokens = tokenize(cmd_line, &count);
            int idx = 0;
            while (idx < count) {
                execute_statement(tokens, count, &idx);
            }
            free_tokens(tokens, count);
            
            if (http_server.running) {
                printf("\nPress Enter to stop server...\n");
                getchar();
                http_server.running = false;
                pthread_join(http_server.thread, NULL);
            }
            return 0;
        }
        if (strcmp(argv[1], "compile") == 0) {
            if (argc < 3) {
                printf("Usage: zenith compile <file.zt> [-o output]\n");
                return 1;
            }
            
            char *input_file = argv[2];
            char *output_file = "a.out";
            bool use_tcc = false;
            
            for (int i = 3; i < argc; i++) {
                if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
                    output_file = argv[++i];
                } else if (strcmp(argv[i], "--tcc") == 0) {
                    use_tcc = true;
                }
            }
            
            FILE *f = fopen(input_file, "r");
            if (!f) {
                printf("Error: Cannot open file '%s'\n", input_file);
                return 1;
            }
            fseek(f, 0, SEEK_END);
            long fsize = ftell(f);
            fseek(f, 0, SEEK_SET);
            char *content = malloc(fsize + 1);
            fread(content, 1, fsize, f);
            content[fsize] = 0;
            fclose(f);
            
            char *escaped = malloc(fsize * 2 + 1024);
            escape_string_for_c(content, escaped, fsize * 2 + 1024);
            
            char c_filename[256];
            snprintf(c_filename, 256, "%s.c", output_file);
            f = fopen(c_filename, "w");
            if (!f) {
                printf("Error: Cannot write to '%s'\n", c_filename);
                free(content); free(escaped);
                return 1;
            }
            
            fprintf(f, "#define ZENITH_NO_MAIN\n");
            fprintf(f, "#include \"zenith.c\"\n\n");
            fprintf(f, "int main(int argc, char *argv[]) {\n");
            fprintf(f, "    char *script = \"%s\";\n", escaped);
            fprintf(f, "    int count;\n");
            fprintf(f, "    Token *tokens = tokenize(script, &count);\n");
            fprintf(f, "    int idx = 0;\n");
            fprintf(f, "    while (idx < count) execute_statement(tokens, count, &idx);\n");
            fprintf(f, "    free_tokens(tokens, count);\n");
            fprintf(f, "    return 0;\n");
            fprintf(f, "}\n");
            fclose(f);
            
            printf("🔨 Compiling '%s' -> '%s'...\n", input_file, output_file);
            
            char cmd[MAX_LINE];
            snprintf(cmd, MAX_LINE, "gcc -O2 -o %s %s -lm -pthread", output_file, c_filename);
            
#ifdef HAVE_SDL2
            strcat(cmd, " -lSDL2 -lGL");
#endif
#ifdef HAVE_TCC
            strcat(cmd, " -ltcc -ldl");
#endif
             strcat(cmd, " -lssl -lcrypto");

            int ret = system(cmd);
            if (ret == 0) {
                printf("✅ Compilation successful: ./%s\n", output_file);
                remove(c_filename); // Clean up temp C file
            } else {
                printf("❌ Compilation failed.\n");
            }
            
            free(content);
            free(escaped);
            return ret;
        }
        
        execute_file(argv[1]);
        
#ifdef HAVE_SDL2
        for (int i = 0; i < window_count; i++) {
            if (windows[i].running) {
                while (windows[i].running) {
                    graphics_poll_events(&windows[i]);
                    SDL_Delay(16);
                }
            }
        }
#endif
        return 0;
    }
    
    repl_mode = true;
    print_version();
    printf("\nInteractive Shell - Type 'exit' to quit\n");
    printf("Features: crypto, graphics, http, files, modules\n\n");
    
    char buffer[MAX_LINE];
    while (1) {
        printf(">>> ");
        fflush(stdout);
        
        if (!fgets(buffer, MAX_LINE, stdin)) break;
        
        trim(buffer);
        if (strcmp(buffer, "exit") == 0 || strcmp(buffer, "quit") == 0) break;
        if (strlen(buffer) == 0) continue;
        
        int count;
        Token *tokens = tokenize(buffer, &count);
        int idx = 0;
        while (idx < count) {
            execute_statement(tokens, count, &idx);
        }
        free_tokens(tokens, count);
    }
    
    http_server.running = false;
    
#ifdef HAVE_SDL2
    for (int i = 0; i < window_count; i++) {
        if (windows[i].window) {
            if (windows[i].use_opengl && windows[i].gl_context) {
                SDL_GL_DeleteContext(windows[i].gl_context);
            } else if (windows[i].renderer) {
                SDL_DestroyRenderer(windows[i].renderer);
            }
            SDL_DestroyWindow(windows[i].window);
        }
    }
    SDL_Quit();
#endif
    
    return 0;
}
#endif