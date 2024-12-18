#include "lexers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

size_t line_number = 1;        // Global line number


Token *create_token(TokenType type, const char *value, size_t line_num) {
    Token *token = (Token *)malloc(sizeof(Token));
    token->type = type;
    token->value = strdup(value);
    token->line_num = line_num;
    return token;
}


const char* token_type_to_string(TokenType type) {
    switch (type) {
        case TOKEN_KEYWORD: return "KEYWORD";
        case TOKEN_RESERVED_WORD: return "RESERVED_WORD";
        case TOKEN_IDENTIFIER: return "IDENTIFIER";
        case TOKEN_INTEGER: return "INTEGER";
        case TOKEN_FLOAT: return "FLOAT";     
        case TOKEN_ARITHMETIC_OPERATOR: return "ARITHMETIC_OPERATOR";
        case TOKEN_BOOLEAN_OPERATOR_RELATIONAL: return "BOOLEAN_OPERATOR_RELATIONAL";
        case TOKEN_BOOLEAN_OPERATOR_LOGICAL: return "BOOLEAN_OPERATOR_LOGICAL";
        case TOKEN_ASSIGNMENT_OPERATOR: return "ASSIGNMENT_OPERATOR";
        case TOKEN_DELIMITER: return "DELIMITER";
        case TOKEN_DELIMITER_OPEN_PARENTHESIS:return "DELIMITER_OPEN_PARENTHESIS";
        case TOKEN_DELIMITER_CLOSE_PARENTHESIS: return "DELIMITER_CLOSE_PARENTHESIS";
        case TOKEN_DELIMITER_OPEN_BRACE: return "DELIMITER_OPEN_BRACE";
        case TOKEN_DELIMITER_CLOSE_BRACE: return "DELIMITER_CLOSE_BRACE";
        case TOKEN_DELIMITER_OPEN_BRACKET: return "DELIMITER_OPEN_BRACKET";
        case TOKEN_DELIMITER_CLOSE_BRACKET: return "DELIMITER_CLOSE_BRACKET";
        case TOKEN_UNARY_OPERATOR: return "UNARY_OPERATOR";
        case TOKEN_NOISE_WORD: return "NOISE_WORD";
        case TOKEN_COMMENT: return "COMMENT";
        case TOKEN_STRING: return "STRING";
        case TOKEN_CHARACTER: return "CHARACTER";
        case TOKEN_UNKNOWN: return "UNKNOWN";
        case TOKEN_EOF: return "EOF";
        default: return "UNDEFINED";
    }
}


void print_token(const Token *token) {
    printf("TOKEN: %s | TYPE: %s | LINE: %zu\n", 
           token->value, 
           token_type_to_string(token->type), 
           token->line_num);
}


void write_to_symbol_table(const Token *token, FILE *symbol_table_file) {
    fprintf(symbol_table_file, "TOKEN: %-20s TYPE: %-20s LINE: %zu\n", 
            token->value, 
            token_type_to_string(token->type), 
            token->line_num);
}


void free_token(Token *token) {
    if (token->value) free(token->value);
    free(token);
}


Token *classify_number(const char *source, int *index) {
    char buffer[64] = {0};
    int buffer_index = 0;
    int has_decimal = 0;

    // Extract the number, allowing one decimal point
    while (isdigit(source[*index]) || (source[*index] == '.' && !has_decimal)) {
        if (source[*index] == '.') {
            has_decimal = 1;
        }
        buffer[buffer_index++] = source[(*index)++];
    }

    // Determine the token type based on the presence of a decimal point
    TokenType type = has_decimal ? TOKEN_FLOAT : TOKEN_INTEGER;

    return create_token(type, buffer, line_number);
}


Token *classify_string(const char *source, int *index) {
    char buffer[256] = {0};
    int buffer_index = 0;

    (*index)++; // Skip the opening quote

    while (source[*index] != '"' && source[*index] != '\0') {
        if (source[*index] == '\n') {
            line_number++; // Handle multi-line strings, if allowed
        }
        buffer[buffer_index++] = source[(*index)++];
    }

    if (source[*index] == '"') {
        (*index)++; // Skip the closing quote
    } else {
        fprintf(stderr, "Unterminated string at line %zu\n", line_number);
    }

    return create_token(TOKEN_STRING, buffer, line_number);
}


Token *classify_character(const char *source, int *index) {
    // Ensure the first character is a single quote
    if (source[*index] != '\'') {
        fprintf(stderr, "Error: Expected single quote at line %zu\n", line_number);
        return NULL;
    }

    (*index)++; // Move past the opening single quote

    char character = source[*index];

    // Validate that the character is not another single quote (to avoid empty or invalid characters)
    if (character == '\0' || character == '\'') {
        fprintf(stderr, "Error: Invalid or empty character at line %zu\n", line_number);
        return NULL;
    }

    (*index)++; // Move past the character

    // Ensure the next character is a closing single quote
    if (source[*index] != '\'') {
        fprintf(stderr, "Error: Expected closing single quote at line %zu\n", line_number);
        return NULL;
    }

    (*index)++; // Move past the closing single quote

    // Convert the character to a string and return it as a token
    char buffer[2] = {character, '\0'};
    return create_token(TOKEN_CHARACTER, buffer, line_number);
}


Token *classify_comment(const char *source, int *index) {
    char buffer[256] = {0};  // Buffer to hold the comment content
    int buffer_index = 0;
    size_t start_line = line_number;  // Track start line of the comment

    // Single-line comment: starts with `~~`
    if (source[*index] == '~' && source[*index + 1] == '~') {
        *index += 2;  // Skip the `~~`
        while (source[*index] != '\n' && source[*index] != '\0') {
            buffer[buffer_index++] = source[(*index)++];
        }
        return create_token(TOKEN_COMMENT, buffer, line_number);  // single-line comment
    }

    // Multi-line comment: starts with `~^` and ends with `^~`
    if (source[*index] == '~' && source[*index + 1] == '^') {
        *index += 2;  // Skip the `~^`
        while (!(source[*index] == '^' && source[*index + 1] == '~') && source[*index] != '\0') {
            if (source[*index] == '\n') {
                line_number++;  // Increment the line number for each new line
                buffer[buffer_index++] = ' ';  // Add a space instead of a newline
            } else {
                buffer[buffer_index++] = source[*index];  // Add other characters normally
            }
            (*index)++;
        }

        // Ensure we skip the closing `^~` if found
        if (source[*index] == '^' && source[*index + 1] == '~') {
            *index += 2;
        } else {
            printf("Warning: Unterminated multi-line comment at line %zu\n", line_number);
        }

        return create_token(TOKEN_COMMENT, buffer, start_line);
    }

    return NULL;  // Not a comment
}


Token *classify_unknown(char c) {
    char unknown[2] = {c, '\0'};
    return create_token(TOKEN_UNKNOWN, unknown, line_number);
}


Token *classify_word(const char *lexeme) {
    int startIdx = 0;

    switch (lexeme[startIdx]) {
        case 'b':
            switch (lexeme[startIdx + 1]) {
                case 'r':
                    if (lexeme[startIdx + 2] == 'e' && lexeme[startIdx + 3] == 'a' && lexeme[startIdx + 4] == 'k' && 
                    lexeme[startIdx + 5] == '\0') {
                        return create_token(TOKEN_KEYWORD, "BREAK", line_number); // "break"
                    }
                    break;
                case 'o':
                    if (lexeme[startIdx + 2] == 'o' && lexeme[startIdx + 3] == 'l' && lexeme[startIdx + 4] == 'e' && 
                    lexeme[startIdx + 5] == 'a' && lexeme[startIdx + 6] == 'n' && lexeme[startIdx + 7] == '\0') {
                        return create_token(TOKEN_RESERVED_WORD, "BOOLEAN", line_number); // "boolean"
                    }
                    break;
            }
            break;
        
        case 'c': // Handles words starting with 'c'
            switch (lexeme[startIdx + 1]) {
                case 'h': // "character"
                    if (lexeme[startIdx + 2] == 'a' && lexeme[startIdx + 3] == 'r' &&
                        lexeme[startIdx + 4] == 'a' && lexeme[startIdx + 5] == 'c' &&
                        lexeme[startIdx + 6] == 't' && lexeme[startIdx + 7] == 'e' &&
                        lexeme[startIdx + 8] == 'r' && lexeme[startIdx + 9] == '\0') {
                        return create_token(TOKEN_RESERVED_WORD, "CHARACTER", line_number); // "character"
                    }
                    break;
                case 'o': 
                    switch (lexeme[startIdx + 2]) {
                        case 'n': 
                            switch (lexeme[startIdx + 3]) {
                                case 's': 
                                    if (lexeme[startIdx + 4] == 't' && lexeme[startIdx + 5] == 'a' &&
                                        lexeme[startIdx + 6] == 'n' && lexeme[startIdx + 7] == 't' &&
                                        lexeme[startIdx + 8] == '\0') {
                                        return create_token(TOKEN_RESERVED_WORD, "CONSTANT", line_number); // "constant"
                                    }
                                    break;
                                case 't': 
                                    if (lexeme[startIdx + 4] == 'i' && lexeme[startIdx + 5] == 'n' &&
                                        lexeme[startIdx + 6] == 'u' && lexeme[startIdx + 7] == 'e' &&
                                        lexeme[startIdx + 8] == '\0') {
                                        return create_token(TOKEN_KEYWORD, "CONTINUE", line_number);// "continue"
                                    }
                                    break;
                            }
                            break;
                    }
                    break;
            }
            break;
        case 'd':
            switch (lexeme[startIdx + 1]) {
                case 'o':
                    if (lexeme[startIdx + 2] == '\0') {
                            return create_token(TOKEN_NOISE_WORD, "DO", line_number); // "do"
                    }
                    break;
                case 'e':
                    if (lexeme[startIdx + 2] == 'f' && lexeme[startIdx + 3] == 'a' &&
                        lexeme[startIdx + 4] == 'u' && lexeme[startIdx + 5] == 'l' &&
                        lexeme[startIdx + 6] == 't' && lexeme[startIdx + 7] == '\0') {
                            return create_token(TOKEN_KEYWORD, "DEFAULT", line_number); // "default"
                    }
                    break;
                case 'i':
                    if (lexeme[startIdx + 2] == 's' && lexeme[startIdx + 3] == 'p' &&
                        lexeme[startIdx + 4] == 'l' && lexeme[startIdx + 5] == 'a' &&
                        lexeme[startIdx + 6] == 'y' && lexeme[startIdx + 7] == '\0') {
                            return create_token(TOKEN_KEYWORD, "DISPLAY", line_number); // "display"
                    }
                    break;
            }
            break;
        
        case 'e':
            switch (lexeme[startIdx + 1]) {
                case 'l':
                    if (lexeme[startIdx + 2] == 's' && lexeme[startIdx + 3] == 'e' &&
                        lexeme[startIdx + 4] == '\0') {
                            return create_token(TOKEN_KEYWORD, "ELSE", line_number); // "else"
                    }
                    break;
                case 'n':
                    if (lexeme[startIdx + 2] == 'd' && lexeme[startIdx + 3] == '\0') {
                    return create_token(TOKEN_NOISE_WORD, "END", line_number);// "end"
                }
                break;
            }
            break;
        case 'f':
            switch (lexeme[startIdx + 1]) {
                case 'o':
                    if (lexeme[startIdx + 2] == 'r' && lexeme[startIdx + 3] == '\0') {
                        return create_token(TOKEN_KEYWORD, "FOR", line_number); // "for"
                    }
                    break;
                case 'l':
                    if (lexeme[startIdx + 2] == 'o' && lexeme[startIdx + 3] == 'a' && lexeme[startIdx + 4] == 't' && lexeme[startIdx + 5] == '\0') {
                        return create_token(TOKEN_RESERVED_WORD, "FLOAT", line_number); // "float"
                    }
                    break;
                case 'a':
                    if (lexeme[startIdx + 2] == 'l' && lexeme[startIdx + 3] == 's' && lexeme[startIdx + 4] == 'e' && lexeme[startIdx + 5] == '\0') {
                        return create_token(TOKEN_RESERVED_WORD, "FALSE", line_number); // "false"
                    }
                    break;
            }
            break;
        case 'i':
            switch (lexeme[startIdx + 1]) {
                case 'f':
                    if (lexeme[startIdx + 2] == '\0') {
                        return create_token(TOKEN_KEYWORD, "IF", line_number); // "if"
                    }
                    break;
                case 'n':
                    switch (lexeme[startIdx + 2]) {
                        case 't':
                            if (lexeme[startIdx + 3] == 'e' && lexeme[startIdx + 4] == 'g' &&
                                lexeme[startIdx + 5] == 'e' && lexeme[startIdx + 6] == 'r' && lexeme[startIdx + 7] == '\0') {
                                return create_token(TOKEN_RESERVED_WORD, "INTEGER", line_number); // "integer"
                            }
                            break;
                        case 'p':
                            if (lexeme[startIdx + 3] == 'u' && lexeme[startIdx + 4] == 't' &&
                                lexeme[startIdx + 5] == '\0') {
                                return create_token(TOKEN_KEYWORD, "INPUT", line_number); // "input"
                            }
                            break;
                    }
                    
            }
            break;
        
        case 'l':
            switch (lexeme[startIdx + 1]) {
                case 'e':
                    if (lexeme[startIdx + 2] == 't' && lexeme[startIdx + 3] == '\0') {
                            return create_token(TOKEN_NOISE_WORD, "LET", line_number);// "let"
                    }
                    break;
            }
            break;
        
        case 'm':
            switch (lexeme[startIdx + 1]) {
                case 'a':
                    if (lexeme[startIdx + 2] == 'i' && lexeme[startIdx + 3] == 'n' &&
                        lexeme[startIdx + 4] == '\0') {
                            return create_token(TOKEN_KEYWORD, "MAIN", line_number); // "main"
                    }
                    break;
            }
            break;

        case 'n':
            switch (lexeme[startIdx + 1]) {
                case 'u':
                    if (lexeme[startIdx + 2] == 'l' && lexeme[startIdx + 3] == 'l' &&
                        lexeme[startIdx + 4] == '\0') {
                            return create_token(TOKEN_RESERVED_WORD, "NULL", line_number); // "null"
                    }
                    break;
            }
            break;
        
        case 'r':
            switch (lexeme[startIdx + 1]) {
                case 'e':
                    if (lexeme[startIdx + 2] == 't' && lexeme[startIdx + 3] == 'u' &&
                        lexeme[startIdx + 4] == 'r' && lexeme[startIdx + 5] == 'n' &&
                        lexeme[startIdx + 6] == '\0') {
                            return create_token(TOKEN_KEYWORD, "RETURN", line_number); // "return"
                    }
                    break;
            }
            break;
        
        case 's':
            switch (lexeme[startIdx + 1]) {
                case 't':
                    if (lexeme[startIdx + 2] == 'r' && lexeme[startIdx + 3] == 'i' &&
                        lexeme[startIdx + 4] == 'n' && lexeme[startIdx + 5] == 'g' &&
                        lexeme[startIdx + 6] == '\0') {
                            return create_token(TOKEN_RESERVED_WORD, "STRING", line_number); // "string"
                    }
                    break;
            }
            break;
        case 't':
            switch (lexeme[startIdx + 1]) {
                case 'h':
                    if (lexeme[startIdx + 2] == 'e' && lexeme[startIdx + 3] == 'n' &&
                        lexeme[startIdx + 4] == '\0') {
                            return create_token(TOKEN_NOISE_WORD, "THEN", line_number); // "then"
                    }
                    break;
                case 'r':
                    if (lexeme[startIdx + 2] == 'u' && lexeme[startIdx + 3] == 'e' &&
                        lexeme[startIdx + 4] == '\0') {
                            return create_token(TOKEN_RESERVED_WORD, "TRUE", line_number); // "true"
                    }
                    break;
            }
            break;
        
        case 'v':
            switch (lexeme[startIdx + 1]) {
                case 'o':
                    if (lexeme[startIdx + 2] == 'i' && lexeme[startIdx + 3] == 'd' &&
                        lexeme[startIdx + 4] == '\0') {
                            return create_token(TOKEN_RESERVED_WORD, "VOID", line_number);// "void"
                    }
                    break;
            }
            break;
        case 'w':
            switch (lexeme[startIdx + 1]) {
                case 'h':
                    if (lexeme[startIdx + 2] == 'i' && lexeme[startIdx + 3] == 'l' &&
                        lexeme[startIdx + 4] == 'e' &&  lexeme[startIdx + 5] == '\0') {
                            return create_token(TOKEN_KEYWORD, "WHILE", line_number); // "while"
                    }
                    break;
            }
            break;
        default: 
            break;
    }
    // If no keyword is matched, classify as an identifier
    return create_token(TOKEN_IDENTIFIER, lexeme, line_number);
    
}


Token *classify_operator(const char *source, int *index) {
    char current = source[*index];
    char next = source[*index + 1];
    (*index)++; // Advance index for single-character operators by default

    switch (current) {
        // Relational Operators
        case '<':
            if (next == '=') {
                (*index)++;
                return create_token(TOKEN_BOOLEAN_OPERATOR_RELATIONAL, "<=", line_number);
            }
            return create_token(TOKEN_BOOLEAN_OPERATOR_RELATIONAL, "<", line_number);

        case '>':
            if (next == '=') {
                (*index)++;
                return create_token(TOKEN_BOOLEAN_OPERATOR_RELATIONAL, ">=", line_number);
            }
            return create_token(TOKEN_BOOLEAN_OPERATOR_RELATIONAL, ">", line_number);

        case '=':
            if (next == '=') {
                (*index)++;
                return create_token(TOKEN_BOOLEAN_OPERATOR_RELATIONAL, "==", line_number);
            }
            return create_token(TOKEN_ASSIGNMENT_OPERATOR, "=", line_number);

        case '!':
            if (next == '=') {
                (*index)++;
                return create_token(TOKEN_BOOLEAN_OPERATOR_RELATIONAL, "!=", line_number);
            }
            return create_token(TOKEN_BOOLEAN_OPERATOR_LOGICAL, "!", line_number);

        // Logical Operators
        case '&':
            if (next == '&') {
                (*index)++;
                return create_token(TOKEN_BOOLEAN_OPERATOR_LOGICAL, "&&", line_number);
            }
            break;

        case '|':
            if (next == '|') {
                (*index)++;
                return create_token(TOKEN_BOOLEAN_OPERATOR_LOGICAL, "||", line_number);
            }
            break;

        // Arithmetic Operators and Unary Operators
        case '+':
            if (next == '=') {
                (*index)++;
                return create_token(TOKEN_ASSIGNMENT_OPERATOR, "+=", line_number);
            }
            if (next == '+') {
                (*index)++;
                return create_token(TOKEN_UNARY_OPERATOR, "++", line_number);
            }
            return create_token(TOKEN_ARITHMETIC_OPERATOR, "+", line_number);

        case '-':
            if (next == '=') {
                (*index)++;
                return create_token(TOKEN_ASSIGNMENT_OPERATOR, "-=", line_number);
            }
            if (next == '-') {
                (*index)++;
                return create_token(TOKEN_UNARY_OPERATOR, "--", line_number);
            }
            return create_token(TOKEN_ARITHMETIC_OPERATOR, "-", line_number);

        case '*':
            if (next == '=') {
                (*index)++;
                return create_token(TOKEN_ASSIGNMENT_OPERATOR, "*=", line_number);
            }
            return create_token(TOKEN_ARITHMETIC_OPERATOR, "*", line_number);

        case '/':
            if (next == '=') {
                (*index)++;
                return create_token(TOKEN_ASSIGNMENT_OPERATOR, "/=", line_number);
            }
            return create_token(TOKEN_ARITHMETIC_OPERATOR, "/", line_number);

        case '$':
            if (next == '=') {
                (*index)++;
                return create_token(TOKEN_ASSIGNMENT_OPERATOR, "$=", line_number);
            }
            return create_token(TOKEN_ARITHMETIC_OPERATOR, "$", line_number);

        case '%':
            if (next == '=') {
                (*index)++;
                return create_token(TOKEN_ASSIGNMENT_OPERATOR, "%=", line_number);
            }
            return create_token(TOKEN_ARITHMETIC_OPERATOR, "%", line_number);

        case '^':
            return create_token(TOKEN_ARITHMETIC_OPERATOR, "^", line_number);

        // Default case for unknown operators
        default: {
            char unknown[2] = {current, '\0'};
            return create_token(TOKEN_UNKNOWN, unknown, line_number);
        }
    }

    // Return NULL for unrecognized multi-character sequences (safety fallback)
    return NULL;
}


Token *classify_delimiter(char c, size_t line_number) {
    switch (c) {
        case ';': return create_token(TOKEN_DELIMITER, ";", line_number);
        case ',': return create_token(TOKEN_DELIMITER, ",", line_number);

        // Parentheses
        case '(': return create_token(TOKEN_DELIMITER_OPEN_PARENTHESIS, "(", line_number);
        case ')': return create_token(TOKEN_DELIMITER_CLOSE_PARENTHESIS, ")", line_number);

        // Braces
        case '{': return create_token(TOKEN_DELIMITER_OPEN_BRACE, "{", line_number);
        case '}': return create_token(TOKEN_DELIMITER_CLOSE_BRACE, "}", line_number);

        // Brackets
        case '[': return create_token(TOKEN_DELIMITER_OPEN_BRACKET, "[", line_number);
        case ']': return create_token(TOKEN_DELIMITER_CLOSE_BRACKET, "]", line_number);

        // Default case for unknown delimiters
        default: {
            char unknown[2] = {c, '\0'};
            return create_token(TOKEN_UNKNOWN, unknown, line_number);
        }
    }
}


Token **tokenize(const char *source, size_t *token_count) {
    size_t capacity = 10;
    Token **tokens = malloc(capacity * sizeof(Token *));
    *token_count = 0;
    int index = 0;
    int length = strlen(source);

    while (index < length) {
        char c = source[index];

        // Skip white spaces and track line numbers
        if (isspace(c)) {
            if (c == '\n') line_number++;
            index++;
            continue;
        }

        Token *token = NULL;

            if (source[index] == '~') {
                Token *comment_token = classify_comment(source, &index);
            if (comment_token) {
                    tokens[*token_count] = comment_token;
                    (*token_count)++;
                    continue;
                }
        }

        // Numbers
        if (isdigit(c)) {
            token = classify_number(source, &index);
        }
        // Keywords or Identifiers
        else if (isalpha(c) || c == '_') {
            char buffer[64] = {0};
            int buffer_index = 0;

            while (isalnum(source[index]) || source[index] == '_') {
                buffer[buffer_index++] = source[index++];
            }

            token = classify_word(buffer); // Call the improved function
        }
        // Operators
        else if (strchr("+-*/=$%^<>!&|", c)) {  // Include new operators ($, %, ^)
            token = classify_operator(source, &index);
        }
        // Delimiters
        else if (strchr(";{},()[]", c)) {
            token = classify_delimiter(c, line_number);
            index++;  // Advance after handling delimiter
        }
        else if (c == '"') { // Detect the start of a string
            token = classify_string(source, &index);
        }
        else if (source[index] == '\'') { // Detect the start of a character
            token = classify_character(source, &index);
        }
        // Handle unrecognized characters
        else {
            fprintf(stderr, "Unrecognized character '%c' at line %zu\n", c, line_number);
            index++;
            continue;
        }

        // Store token
        if (token) {
            if (*token_count == capacity) {
                capacity *= 2;
                tokens = realloc(tokens, capacity * sizeof(Token *));
            }
            tokens[(*token_count)++] = token;
        }
    }

    // Add END_OF_TOKENS marker
    tokens[*token_count] = create_token(TOKEN_EOF, "EOF", line_number);
    return tokens;
}


Token **lexer(FILE *file, size_t *token_count) {
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);

    char *buffer = malloc(file_size + 1);
    fread(buffer, 1, file_size, file);
    buffer[file_size] = '\0';

    Token **tokens = tokenize(buffer, token_count);
    free(buffer);

    return tokens;
}
