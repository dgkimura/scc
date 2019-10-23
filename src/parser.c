/*
 * Provides a bottom-up CLR(1) parser. This involves 3 main steps:
 *
 * 1) Given a grammar, construct a state machine. This performed in
 *    `generate_states()`.
 * 2) Given a state machine, construct a parse table. This is performed in
 *    `init_parsetable()`.
 * 3) Given a parse table, construct the AST (abstract syntax tree). This is
 *    performed in `parse()`.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "utilities.h"

static struct parsetable_item *parsetable = NULL;

/*
 * state_identifier indicates the total number of states in the grammar.
 */
static int state_identifier = 0;
static int tmp_state_identifier = 0;

/*
 * states is the list of states in the grammar.
 */
static struct state states[MAX_STATES];

static struct state temp_states[MAX_STATES];

#define NUM_RULES 199

/*
 * List of rules that defines the grammar for the parser. It is a copy of the C
 * grammar as defined by K&R in "C Programming Language" 2nd edition reference
 * manual.
 */
struct rule grammar[NUM_RULES] =
{
    /* translation-unit: */
    {
        AST_TRANSLATION_UNIT,
        1,
        { AST_EXTERNAL_DECLARATION }
    },
    {
        AST_TRANSLATION_UNIT,
        2,
        { AST_TRANSLATION_UNIT, AST_EXTERNAL_DECLARATION }
    },
    /* external-declaration: */
    {
        AST_EXTERNAL_DECLARATION,
        1,
        { AST_FUNCTION_DEFINITION }
    },
    {
        AST_EXTERNAL_DECLARATION,
        1,
        { AST_DECLARATION }
    },
    /* function-definition: */
    {
        AST_FUNCTION_DEFINITION,
        2,
        { AST_DECLARATOR, AST_COMPOUND_STATEMENT }
    },
    {
        AST_FUNCTION_DEFINITION,
        3,
        { AST_DECLARATION_SPECIFIERS, AST_DECLARATOR, AST_COMPOUND_STATEMENT }
    },
    {
        AST_FUNCTION_DEFINITION,
        3,
        { AST_DECLARATOR, AST_DECLARATION_LIST, AST_COMPOUND_STATEMENT }
    },
    {
        AST_FUNCTION_DEFINITION,
        4,
        { AST_DECLARATION_SPECIFIERS, AST_DECLARATOR, AST_DECLARATION_LIST, AST_COMPOUND_STATEMENT }
    },
    /* declaration: */
    {
        AST_DECLARATION,
        2,
        { AST_DECLARATION_SPECIFIERS, AST_SEMICOLON }
    },
    {
        AST_DECLARATION,
        3,
        { AST_DECLARATION_SPECIFIERS, AST_INIT_DECLARATOR_LIST, AST_SEMICOLON }
    },
    /* declaration-list: */
    {
        AST_DECLARATION_LIST,
        1,
        { AST_DECLARATION }
    },
    {
        AST_DECLARATION_LIST,
        2,
        { AST_DECLARATION_LIST, AST_DECLARATION }
    },
    /* declaration-specifiers: */
    {
        AST_DECLARATION_SPECIFIERS,
        1,
        { AST_STORAGE_CLASS_SPECIFIER }
    },
    {
        AST_DECLARATION_SPECIFIERS,
        2,
        { AST_STORAGE_CLASS_SPECIFIER, AST_DECLARATION_SPECIFIERS }
    },
    {
        AST_DECLARATION_SPECIFIERS,
        1,
        { AST_TYPE_SPECIFIER }
    },
    {
        AST_DECLARATION_SPECIFIERS,
        2,
        { AST_TYPE_SPECIFIER, AST_DECLARATION_SPECIFIERS }
    },
    {
        AST_DECLARATION_SPECIFIERS,
        1,
        { AST_TYPE_QUALIFIER }
    },
    {
        AST_DECLARATION_SPECIFIERS,
        2,
        { AST_TYPE_QUALIFIER, AST_DECLARATION_SPECIFIERS }
    },
    /* storage-class-specifier: */
    {
        AST_STORAGE_CLASS_SPECIFIER,
        1,
        { AST_AUTO }
    },
    {
        AST_STORAGE_CLASS_SPECIFIER,
        1,
        { AST_REGISTER }
    },
    {
        AST_STORAGE_CLASS_SPECIFIER,
        1,
        { AST_STATIC }
    },
    {
        AST_STORAGE_CLASS_SPECIFIER,
        1,
        { AST_EXTERN }
    },
    {
        AST_STORAGE_CLASS_SPECIFIER,
        1,
        { AST_TYPEDEF }
    },
    /* type-specifier: */
    {
        AST_TYPE_SPECIFIER,
        1,
        { AST_VOID }
    },
    {
        AST_TYPE_SPECIFIER,
        1,
        { AST_CHAR }
    },
    {
        AST_TYPE_SPECIFIER,
        1,
        { AST_SHORT }
    },
    {
        AST_TYPE_SPECIFIER,
        1,
        { AST_INT }
    },
    {
        AST_TYPE_SPECIFIER,
        1,
        { AST_LONG }
    },
    {
        AST_TYPE_SPECIFIER,
        1,
        { AST_FLOAT }
    },
    {
        AST_TYPE_SPECIFIER,
        1,
        { AST_DOUBLE }
    },
    {
        AST_TYPE_SPECIFIER,
        1,
        { AST_SIGNED }
    },
    {
        AST_TYPE_SPECIFIER,
        1,
        { AST_UNSIGNED }
    },
    {
        AST_TYPE_SPECIFIER,
        1,
        { AST_STRUCT_OR_UNION_SPECIFIER }
    },
    {
        AST_TYPE_SPECIFIER,
        1,
        { AST_ENUM_SPECIFIER }
    },
    {
        AST_TYPE_SPECIFIER,
        1,
        { AST_TYPEDEF_NAME }
    },
    /* type-qualifier: */
    {
        AST_TYPE_QUALIFIER,
        1,
        { AST_CONST }
    },
    {
        AST_TYPE_QUALIFIER,
        1,
        { AST_VOLATILE }
    },
    /* struct-or-union-specifier: */
    {
        AST_STRUCT_OR_UNION_SPECIFIER,
        4,
        { AST_STRUCT_OR_UNION, AST_LBRACE, AST_STRUCT_DECLARATION_LIST, AST_RBRACE }
    },
    {
        AST_STRUCT_OR_UNION_SPECIFIER,
        5,
        { AST_STRUCT_OR_UNION, AST_IDENTIFIER, AST_LBRACE, AST_STRUCT_DECLARATION_LIST, AST_RBRACE }
    },
    {
        AST_STRUCT_OR_UNION_SPECIFIER,
        2,
        { AST_STRUCT_OR_UNION, AST_IDENTIFIER }
    },
    /* struct-or-union: */
    {
        AST_STRUCT_OR_UNION,
        1,
        { AST_STRUCT }
    },
    {
        AST_STRUCT_OR_UNION,
        1,
        { AST_UNION }
    },
    /* struct-declaration-list: */
    {
        AST_STRUCT_DECLARATION_LIST,
        1,
        { AST_STRUCT_DECLARATION }
    },
    {
        AST_STRUCT_DECLARATION_LIST,
        2,
        { AST_STRUCT_DECLARATION_LIST, AST_STRUCT_DECLARATION }
    },
    /* init-declarator-list: */
    {
        AST_INIT_DECLARATOR_LIST,
        1,
        { AST_INIT_DECLARATOR }
    },
    {
        AST_INIT_DECLARATOR_LIST,
        3,
        { AST_INIT_DECLARATOR_LIST, AST_COMMA, AST_INIT_DECLARATOR }
    },
    /* init-declarator: */
    {
        AST_INIT_DECLARATOR,
        1,
        { AST_DECLARATOR }
    },
    {
        AST_INIT_DECLARATOR,
        3,
        { AST_DECLARATOR, AST_EQUAL, AST_INITIALIZER }
    },
    /* struct-declaration: */
    {
        AST_STRUCT_DECLARATION,
        3,
        { AST_SPECIFIER_QUALIFIER_LIST, AST_STRUCT_DECLARATION_LIST, AST_SEMICOLON }
    },
    /* specifier-qualifier-list: */
    {
        AST_SPECIFIER_QUALIFIER_LIST,
        1,
        { AST_TYPE_SPECIFIER }
    },
    {
        AST_SPECIFIER_QUALIFIER_LIST,
        2,
        { AST_TYPE_SPECIFIER, AST_SPECIFIER_QUALIFIER_LIST }
    },
    {
        AST_SPECIFIER_QUALIFIER_LIST,
        1,
        { AST_TYPE_QUALIFIER }
    },
    {
        AST_SPECIFIER_QUALIFIER_LIST,
        2,
        { AST_TYPE_QUALIFIER, AST_SPECIFIER_QUALIFIER_LIST }
    },
    /* struct-declaration-list: */
    {
        AST_STRUCT_DECLARATION_LIST,
        1,
        { AST_STRUCT_DECLARATOR }
    },
    {
        AST_STRUCT_DECLARATION_LIST,
        3,
        { AST_STRUCT_DECLARATION_LIST, AST_COMMA, AST_STRUCT_DECLARATOR }
    },
    /* struct-declarator: */
    {
        AST_STRUCT_DECLARATOR,
        1,
        { AST_DECLARATOR }
    },
    {
        AST_STRUCT_DECLARATOR,
        2,
        { AST_COLON, AST_CONSTANT_EXPRESSION }
    },
    {
        AST_STRUCT_DECLARATOR,
        3,
        { AST_DECLARATOR, AST_COLON, AST_CONSTANT_EXPRESSION }
    },
    /* enum-specifier: */
    {
        AST_ENUM_SPECIFIER,
        2,
        { AST_ENUM, AST_IDENTIFIER }
    },
    {
        AST_ENUM_SPECIFIER,
        4,
        { AST_ENUM, AST_LBRACE, AST_ENUMERATOR_LIST, AST_RBRACKET }
    },
    {
        AST_ENUM_SPECIFIER,
        5,
        { AST_ENUM, AST_IDENTIFIER, AST_LBRACE, AST_ENUMERATOR_LIST, AST_RBRACKET }
    },
    /* enumerator-list: */
    {
        AST_ENUMERATOR_LIST,
        1,
        { AST_ENUMERATOR }
    },
    {
        AST_ENUMERATOR_LIST,
        3,
        { AST_ENUMERATOR_LIST, AST_COMMA, AST_ENUMERATOR }
    },
    /* enumerator: */
    {
        AST_ENUMERATOR,
        1,
        { AST_IDENTIFIER }
    },
    {
        AST_ENUMERATOR,
        3,
        { AST_IDENTIFIER, AST_EQUAL, AST_CONSTANT_EXPRESSION }
    },
    /* declarator: */
    {
        AST_DECLARATOR,
        1,
        { AST_DIRECT_DECLARATOR }
    },
    {
        AST_DECLARATOR,
        2,
        { AST_POINTER, AST_DIRECT_DECLARATOR }
    },
    /* direct-declarator: */
    {
        AST_DIRECT_DECLARATOR,
        1,
        { AST_IDENTIFIER }
    },
    {
        AST_DIRECT_DECLARATOR,
        3,
        { AST_LPAREN, AST_DECLARATOR, AST_RPAREN }
    },
    {
        AST_DIRECT_DECLARATOR,
        3,
        { AST_DIRECT_DECLARATOR, AST_LBRACKET, AST_RBRACKET }
    },
    {
        AST_DIRECT_DECLARATOR,
        4,
        { AST_DIRECT_DECLARATOR, AST_LBRACKET, AST_CONSTANT_EXPRESSION, AST_RBRACKET }
    },
    {
        AST_DIRECT_DECLARATOR,
        3,
        { AST_DIRECT_DECLARATOR, AST_LPAREN, AST_RPAREN }
    },
    {
        AST_DIRECT_DECLARATOR,
        4,
        { AST_DIRECT_DECLARATOR, AST_LPAREN, AST_PARAMETER_TYPE_LIST, AST_RPAREN }
    },
    {
        AST_DIRECT_DECLARATOR,
        4,
        { AST_DIRECT_DECLARATOR, AST_LPAREN, AST_IDENTIFIER_LIST, AST_RPAREN }
    },
    /* pointer: */
    {
        AST_POINTER,
        1,
        { AST_ASTERISK }
    },
    {
        AST_POINTER,
        2,
        { AST_ASTERISK, AST_TYPE_QUALIFIER_LIST }
    },
    {
        AST_POINTER,
        2,
        { AST_ASTERISK, AST_POINTER }
    },
    {
        AST_POINTER,
        3,
        { AST_ASTERISK, AST_TYPE_QUALIFIER_LIST, AST_POINTER }
    },
    /* type-qualifier-list: */
    {
        AST_TYPE_QUALIFIER_LIST,
        1,
        { AST_TYPE_QUALIFIER }
    },
    {
        AST_TYPE_QUALIFIER_LIST,
        2,
        { AST_TYPE_QUALIFIER_LIST, AST_TYPE_QUALIFIER }
    },
    /* parameter-type-list: */
    {
        AST_PARAMETER_TYPE_LIST,
        1,
        { AST_PARAMETER_LIST }
    },
    {
        AST_PARAMETER_TYPE_LIST,
        3,
        { AST_PARAMETER_LIST, AST_COMMA, AST_ELLIPSIS }
    },
    /* parameter-list: */
    {
        AST_PARAMETER_LIST,
        1,
        { AST_PARAMETER_DECLARATION }
    },
    {
        AST_PARAMETER_LIST,
        3,
        { AST_PARAMETER_LIST, AST_COMMA, AST_PARAMETER_DECLARATION }
    },
    /* parameter-declaration: */
    {
        AST_PARAMETER_DECLARATION,
        2,
        { AST_DECLARATION_SPECIFIERS, AST_DECLARATOR }
    },
    {
        AST_PARAMETER_DECLARATION,
        2,
        { AST_DECLARATION_SPECIFIERS, AST_ABSTRACT_DECLARATOR }
    },
    {
        AST_PARAMETER_DECLARATION,
        1,
        { AST_DECLARATION_SPECIFIERS }
    },
    /* identifier-list: */
    {
        AST_IDENTIFIER_LIST,
        1,
        { AST_IDENTIFIER }
    },
    {
        AST_IDENTIFIER_LIST,
        3,
        { AST_IDENTIFIER_LIST, AST_COMMA, AST_IDENTIFIER }
    },
    /* initializer: */
    {
        AST_INITIALIZER,
        1,
        { AST_ASSIGNMENT_EXPRESSION }
    },
    {
        AST_INITIALIZER,
        3,
        { AST_LBRACE, AST_INITIALIZER_LIST, AST_RBRACE }
    },
    {
        AST_INITIALIZER,
        4,
        { AST_LBRACE, AST_INITIALIZER_LIST, AST_COMMA, AST_RBRACE }
    },
    /* initializer-list: */
    {
        AST_INITIALIZER_LIST,
        1,
        { AST_INITIALIZER }
    },
    {
        AST_INITIALIZER_LIST,
        3,
        { AST_INITIALIZER_LIST, AST_COMMA, AST_INITIALIZER }
    },
    /* type-name: */
    {
        AST_TYPE_NAME,
        1,
        { AST_SPECIFIER_QUALIFIER_LIST }
    },
    {
        AST_ABSTRACT_DECLARATOR,
        2,
        { AST_SPECIFIER_QUALIFIER_LIST, AST_ABSTRACT_DECLARATOR }
    },
    /* abstract-declarator: */
    {
        AST_ABSTRACT_DECLARATOR,
        1,
        { AST_POINTER }
    },
    {
        AST_ABSTRACT_DECLARATOR,
        1,
        { AST_DIRECT_ABSTRACT_DECLARATOR }
    },
    {
        AST_ABSTRACT_DECLARATOR,
        2,
        { AST_POINTER, AST_DIRECT_ABSTRACT_DECLARATOR }
    },
    /* direct-abstract-declarator: */
    {
        AST_DIRECT_ABSTRACT_DECLARATOR,
        3,
        { AST_LPAREN, AST_ABSTRACT_DECLARATOR, AST_RPAREN }
    },
    {
        AST_DIRECT_ABSTRACT_DECLARATOR,
        2,
        { AST_LBRACKET, AST_RBRACKET }
    },
    {
        AST_DIRECT_ABSTRACT_DECLARATOR,
        3,
        { AST_DIRECT_ABSTRACT_DECLARATOR, AST_LBRACKET, AST_RBRACKET }
    },
    {
        AST_DIRECT_ABSTRACT_DECLARATOR,
        3,
        { AST_LBRACKET, AST_CONSTANT_EXPRESSION, AST_RBRACKET }
    },
    {
        AST_DIRECT_ABSTRACT_DECLARATOR,
        4,
        { AST_DIRECT_ABSTRACT_DECLARATOR, AST_LBRACKET, AST_CONSTANT_EXPRESSION, AST_RBRACKET }
    },
    {
        AST_DIRECT_ABSTRACT_DECLARATOR,
        2,
        { AST_LPAREN, AST_RPAREN }
    },
    {
        AST_DIRECT_ABSTRACT_DECLARATOR,
        3,
        { AST_DIRECT_ABSTRACT_DECLARATOR, AST_LPAREN, AST_RPAREN }
    },
    {
        AST_DIRECT_ABSTRACT_DECLARATOR,
        3,
        { AST_LPAREN, AST_PARAMETER_TYPE_LIST, AST_RPAREN }
    },
    {
        AST_DIRECT_ABSTRACT_DECLARATOR,
        4,
        { AST_DIRECT_ABSTRACT_DECLARATOR, AST_LPAREN, AST_PARAMETER_TYPE_LIST, AST_RPAREN }
    },
    /* statement: */
    {
        AST_STATEMENT,
        1,
        { AST_LABELED_STATEMENT }
    },
    {
        AST_STATEMENT,
        1,
        { AST_EXPRESSION_STATEMENT }
    },
    {
        AST_STATEMENT,
        1,
        { AST_COMPOUND_STATEMENT }
    },
    {
        AST_STATEMENT,
        1,
        { AST_SELECTION_STATEMENT }
    },
    {
        AST_STATEMENT,
        1,
        { AST_ITERATION_STATEMENT }
    },
    {
        AST_STATEMENT,
        1,
        { AST_JUMP_STATEMENT }
    },
    /* labeled-statement: */
    {
        AST_LABELED_STATEMENT,
        3,
        { AST_IDENTIFIER, AST_COLON, AST_STATEMENT }
    },
    {
        AST_LABELED_STATEMENT,
        4,
        { AST_CASE, AST_CONSTANT_EXPRESSION, AST_COLON, AST_STATEMENT }
    },
    {
        AST_LABELED_STATEMENT,
        3,
        { AST_DEFAULT, AST_COLON, AST_STATEMENT }
    },
    /* expression-statement: */
    {
        AST_EXPRESSION_STATEMENT,
        1,
        { AST_SEMICOLON }
    },
    {
        AST_EXPRESSION_STATEMENT,
        2,
        { AST_EXPRESSION, AST_SEMICOLON }
    },
    /* compound-statement: */
    {
        AST_COMPOUND_STATEMENT,
        2,
        { AST_LBRACE, AST_RBRACE }
    },
    {
        AST_COMPOUND_STATEMENT,
        3,
        { AST_LBRACE, AST_DECLARATION_LIST, AST_RBRACE }
    },
    {
        AST_COMPOUND_STATEMENT,
        3,
        { AST_LBRACE, AST_STATEMENT_LIST, AST_RBRACE }
    },
    {
        AST_COMPOUND_STATEMENT,
        4,
        { AST_LBRACE, AST_DECLARATION_LIST, AST_STATEMENT_LIST, AST_RBRACE }
    },
    /* statement-list: */
    {
        AST_STATEMENT_LIST,
        2,
        { AST_STATEMENT_LIST, AST_STATEMENT }
    },
    {
        AST_STATEMENT_LIST,
        1,
        { AST_STATEMENT }
    },
    /* selection-statement: */
    {
        AST_SELECTION_STATEMENT,
        5,
        { AST_IF, AST_LPAREN, AST_EXPRESSION, AST_RPAREN, AST_STATEMENT }
    },
    {
        AST_SELECTION_STATEMENT,
        7,
        { AST_IF, AST_LPAREN, AST_EXPRESSION, AST_RPAREN, AST_STATEMENT, AST_ELSE, AST_STATEMENT }
    },
    {
        AST_SELECTION_STATEMENT,
        5,
        { AST_SWITCH, AST_LPAREN, AST_EXPRESSION, AST_RPAREN, AST_STATEMENT }
    },
    /* iteration-statement: */
    {
        AST_ITERATION_STATEMENT,
        5,
        { AST_WHILE, AST_LPAREN, AST_EXPRESSION, AST_RPAREN, AST_STATEMENT }
    },
    {
        AST_ITERATION_STATEMENT,
        7,
        { AST_DO, AST_STATEMENT, AST_WHILE, AST_LPAREN, AST_EXPRESSION, AST_RPAREN, AST_SEMICOLON }
    },
    {
        AST_ITERATION_STATEMENT,
        6,
        { AST_FOR, AST_LPAREN, AST_SEMICOLON, AST_SEMICOLON, AST_RPAREN, AST_STATEMENT }
    },
    {
        AST_ITERATION_STATEMENT,
        7,
        { AST_FOR, AST_LPAREN, AST_EXPRESSION, AST_SEMICOLON, AST_SEMICOLON, AST_RPAREN, AST_STATEMENT }
    },
    {
        AST_ITERATION_STATEMENT,
        7,
        { AST_FOR, AST_LPAREN, AST_SEMICOLON, AST_EXPRESSION, AST_SEMICOLON, AST_RPAREN, AST_STATEMENT }
    },
    {
        AST_ITERATION_STATEMENT,
        7,
        { AST_FOR, AST_LPAREN, AST_SEMICOLON, AST_SEMICOLON, AST_EXPRESSION, AST_RPAREN, AST_STATEMENT }
    },
    {
        AST_ITERATION_STATEMENT,
        8,
        { AST_FOR, AST_LPAREN, AST_EXPRESSION, AST_SEMICOLON, AST_EXPRESSION, AST_SEMICOLON, AST_RPAREN, AST_STATEMENT }
    },
    {
        AST_ITERATION_STATEMENT,
        8,
        { AST_FOR, AST_LPAREN, AST_EXPRESSION, AST_SEMICOLON, AST_SEMICOLON, AST_EXPRESSION, AST_RPAREN, AST_STATEMENT }
    },
    {
        AST_ITERATION_STATEMENT,
        8,
        { AST_FOR, AST_LPAREN, AST_SEMICOLON, AST_EXPRESSION, AST_SEMICOLON, AST_EXPRESSION, AST_RPAREN, AST_STATEMENT }
    },
    {
        AST_ITERATION_STATEMENT,
        9,
        { AST_FOR, AST_LPAREN, AST_EXPRESSION, AST_SEMICOLON, AST_EXPRESSION, AST_SEMICOLON, AST_EXPRESSION, AST_RPAREN, AST_STATEMENT }
    },
    /* jump-statement: */
    {
        AST_JUMP_STATEMENT,
        3,
        { AST_GOTO, AST_IDENTIFIER, AST_SEMICOLON }
    },
    {
        AST_JUMP_STATEMENT,
        2,
        { AST_CONTINUE, AST_SEMICOLON }
    },
    {
        AST_JUMP_STATEMENT,
        2,
        { AST_BREAK, AST_SEMICOLON }
    },
    {
        AST_JUMP_STATEMENT,
        2,
        { AST_RETURN, AST_SEMICOLON }
    },
    {
        AST_JUMP_STATEMENT,
        3,
        { AST_RETURN, AST_EXPRESSION, AST_SEMICOLON }
    },
    /* expression: */
    {
        AST_EXPRESSION,
        3,
        { AST_EXPRESSION, AST_COMMA, AST_ASSIGNMENT_EXPRESSION }
    },
    {
        AST_EXPRESSION,
        1,
        { AST_ASSIGNMENT_EXPRESSION }
    },
    /* assignment-expression: */
    /* TODO: other assignemnt operators*/
    {
        AST_ASSIGNMENT_EXPRESSION,
        3,
        { AST_UNARY_EXPRESSION, AST_EQUAL, AST_ASSIGNMENT_EXPRESSION }
    },
    {
        AST_ASSIGNMENT_EXPRESSION,
        3,
        { AST_UNARY_EXPRESSION, AST_ASTERISK_EQUAL, AST_ASSIGNMENT_EXPRESSION }
    },
    {
        AST_ASSIGNMENT_EXPRESSION,
        3,
        { AST_UNARY_EXPRESSION, AST_BACKSLASH_EQUAL, AST_ASSIGNMENT_EXPRESSION }
    },
    {
        AST_ASSIGNMENT_EXPRESSION,
        3,
        { AST_UNARY_EXPRESSION, AST_MOD_EQUAL, AST_ASSIGNMENT_EXPRESSION }
    },
    {
        AST_ASSIGNMENT_EXPRESSION,
        3,
        { AST_UNARY_EXPRESSION, AST_PLUS_EQUAL, AST_ASSIGNMENT_EXPRESSION }
    },
    {
        AST_ASSIGNMENT_EXPRESSION,
        3,
        { AST_UNARY_EXPRESSION, AST_MINUS_EQUAL, AST_ASSIGNMENT_EXPRESSION }
    },
    {
        AST_ASSIGNMENT_EXPRESSION,
        1,
        { AST_CONDITIONAL_EXPRESSION }
    },
    /* constant-expression: */
    {
        AST_CONSTANT_EXPRESSION,
        1,
        { AST_CONDITIONAL_EXPRESSION }
    },
    /* conditional-expression: */
    {
        AST_CONDITIONAL_EXPRESSION,
        5,
        { AST_LOGICAL_OR_EXPRESSION, AST_QUESTIONMARK, AST_EXPRESSION, AST_COLON, AST_CONDITIONAL_EXPRESSION }
    },
    {
        AST_CONDITIONAL_EXPRESSION,
        1,
        { AST_LOGICAL_OR_EXPRESSION }
    },
    /* logical-or-expression: */
    {
        AST_LOGICAL_OR_EXPRESSION,
        3,
        { AST_LOGICAL_OR_EXPRESSION, AST_VERTICALBAR_VERTICALBAR, AST_LOGICAL_AND_EXPRESSION }
    },
    {
        AST_LOGICAL_OR_EXPRESSION,
        1,
        { AST_LOGICAL_AND_EXPRESSION }
    },
    /* logical-and-expression: */
    {
        AST_LOGICAL_AND_EXPRESSION,
        3,
        { AST_LOGICAL_AND_EXPRESSION, AST_AMPERSAND_AMPERSAND, AST_INCLUSIVE_OR_EXPRESSION }
    },
    {
        AST_LOGICAL_AND_EXPRESSION,
        1,
        { AST_INCLUSIVE_OR_EXPRESSION }
    },
    /* inclusive-or-expression: */
    {
        AST_INCLUSIVE_OR_EXPRESSION,
        3,
        { AST_INCLUSIVE_OR_EXPRESSION, AST_VERTICALBAR, AST_EXCLUSIVE_OR_EXPRESSION }
    },
    {
        AST_INCLUSIVE_OR_EXPRESSION,
        1,
        { AST_EXCLUSIVE_OR_EXPRESSION }
    },
    /* exclusive-or-expression: */
    {
        AST_EXCLUSIVE_OR_EXPRESSION,
        3,
        { AST_EXCLUSIVE_OR_EXPRESSION, AST_CARET, AST_AND_EXPRESSION }
    },
    {
        AST_EXCLUSIVE_OR_EXPRESSION,
        1,
        { AST_AND_EXPRESSION }
    },
    /* and-expression: */
    {
        AST_AND_EXPRESSION,
        3,
        { AST_AND_EXPRESSION, AST_AMPERSAND, AST_EQUALITY_EXPRESSION }
    },
    {
        AST_AND_EXPRESSION,
        1,
        { AST_EQUALITY_EXPRESSION }
    },
    /* equality-expression: */
    {
        AST_EQUALITY_EXPRESSION,
        3,
        { AST_EQUALITY_EXPRESSION, AST_EQ, AST_RELATIONAL_EXPRESSION }
    },
    {
        AST_EQUALITY_EXPRESSION,
        3,
        { AST_EQUALITY_EXPRESSION, AST_NEQ, AST_RELATIONAL_EXPRESSION }
    },
    {
        AST_EQUALITY_EXPRESSION,
        1,
        { AST_RELATIONAL_EXPRESSION }
    },
    /* relational-expression: */
    {
        AST_RELATIONAL_EXPRESSION,
        3,
        { AST_RELATIONAL_EXPRESSION, AST_LT, AST_SHIFT_EXPRESSION }
    },
    {
        AST_RELATIONAL_EXPRESSION,
        3,
        { AST_RELATIONAL_EXPRESSION, AST_GT, AST_SHIFT_EXPRESSION }
    },
    {
        AST_RELATIONAL_EXPRESSION,
        3,
        { AST_RELATIONAL_EXPRESSION, AST_LTEQ, AST_SHIFT_EXPRESSION }
    },
    {
        AST_RELATIONAL_EXPRESSION,
        3,
        { AST_RELATIONAL_EXPRESSION, AST_GTEQ, AST_SHIFT_EXPRESSION }
    },
    {
        AST_RELATIONAL_EXPRESSION,
        1,
        { AST_SHIFT_EXPRESSION }
    },
    /* shift-expression: */
    {
        AST_SHIFT_EXPRESSION,
        3,
        { AST_SHIFT_EXPRESSION, AST_SHIFTLEFT, AST_ADDITIVE_EXPRESSION }
    },
    {
        AST_SHIFT_EXPRESSION,
        3,
        { AST_SHIFT_EXPRESSION, AST_SHIFTRIGHT, AST_ADDITIVE_EXPRESSION }
    },
    {
        AST_SHIFT_EXPRESSION,
        1,
        { AST_ADDITIVE_EXPRESSION }
    },
    /* additive-expression: */
    {
        AST_ADDITIVE_EXPRESSION,
        3,
        { AST_ADDITIVE_EXPRESSION, AST_PLUS, AST_MULTIPLICATIVE_EXPRESSION }
    },
    {
        AST_ADDITIVE_EXPRESSION,
        3,
        { AST_ADDITIVE_EXPRESSION, AST_MINUS, AST_MULTIPLICATIVE_EXPRESSION }
    },
    {
        AST_ADDITIVE_EXPRESSION,
        1,
        { AST_MULTIPLICATIVE_EXPRESSION }
    },
    /* multplicative-expression: */
    {
        AST_MULTIPLICATIVE_EXPRESSION,
        3,
        { AST_MULTIPLICATIVE_EXPRESSION, AST_ASTERISK, AST_CAST_EXPRESSION }
    },
    {
        AST_MULTIPLICATIVE_EXPRESSION,
        3,
        { AST_MULTIPLICATIVE_EXPRESSION, AST_BACKSLASH, AST_CAST_EXPRESSION }
    },
    {
        AST_MULTIPLICATIVE_EXPRESSION,
        3,
        { AST_MULTIPLICATIVE_EXPRESSION, AST_MOD, AST_CAST_EXPRESSION }
    },
    {
        AST_MULTIPLICATIVE_EXPRESSION,
        1,
        { AST_CAST_EXPRESSION }
    },
    /* cast-expression: */
    {
        AST_CAST_EXPRESSION,
        1,
        { AST_UNARY_EXPRESSION }
    },
    /* unary-expression: */
    {
        AST_UNARY_EXPRESSION,
        2,
        { AST_PLUS_PLUS, AST_UNARY_EXPRESSION }
    },
    {
        AST_UNARY_EXPRESSION,
        2,
        { AST_MINUS_MINUS, AST_UNARY_EXPRESSION }
    },
    {
        AST_UNARY_EXPRESSION,
        2,
        { AST_AMPERSAND, AST_CAST_EXPRESSION }
    },
    {
        AST_UNARY_EXPRESSION,
        2,
        { AST_ASTERISK, AST_CAST_EXPRESSION }
    },
    {
        AST_UNARY_EXPRESSION,
        2,
        { AST_PLUS, AST_CAST_EXPRESSION }
    },
    {
        AST_UNARY_EXPRESSION,
        2,
        { AST_MINUS, AST_CAST_EXPRESSION }
    },
    {
        AST_UNARY_EXPRESSION,
        1,
        { AST_POSTFIX_EXPRESSION }
    },
    /* postfix-expression: */
    {
        AST_POSTFIX_EXPRESSION,
        3,
        { AST_POSTFIX_EXPRESSION, AST_ARROW, AST_IDENTIFIER }
    },
    {
        AST_POSTFIX_EXPRESSION,
        2,
        { AST_POSTFIX_EXPRESSION, AST_PLUS_PLUS }
    },
    {
        AST_POSTFIX_EXPRESSION,
        2,
        { AST_POSTFIX_EXPRESSION, AST_MINUS_MINUS }
    },
    {
        AST_POSTFIX_EXPRESSION,
        1,
        { AST_PRIMARY_EXPRESSION }
    },
    /* primary-expression: */
    {
        AST_PRIMARY_EXPRESSION,
        1,
        { AST_IDENTIFIER }
    },
    {
        AST_PRIMARY_EXPRESSION,
        1,
        { AST_CONSTANT }
    },
    /* constant: */
    {
        AST_CONSTANT,
        1,
        { AST_INTEGER_CONSTANT }
    },
    {
        AST_CONSTANT,
        1,
        { AST_CHARACTER_CONSTANT }
    },
};

struct rule *
get_grammar(void)
{
    return grammar;
}

static int
checked_nodes_contains(struct listnode **items, enum astnode_t node)
{
    int contains = 0;
    struct listnode *c;

    c = *items;
    while (c != NULL)
    {
        if ((enum astnode_t)c->data == node)
        {
            contains = 1;
            break;
        }

        c = c->next;
    }
    return contains;
}

void
head_terminal_values(enum astnode_t node, struct listnode **checked_nodes,
                     struct listnode **terminals)
{
    int i;

    if (checked_nodes_contains(checked_nodes, node))
    {
        /*
         * If another iteration is already checking node, then there is nothing
         * more to do here.
         */
        return;
    }

    if (node < AST_INVALID)
    {
        /*
         * If node is a terminal symbol then add it and return.
         */
        list_append(terminals, (void *)node);
        return;
    }

    /*
     * Update checked_nodes to avoid trying to repeated work and infinite
     * recursion.
     */
    list_append(checked_nodes, (void *)node);

    for (i=0; i<NUM_RULES; i++)
    {
        if (grammar[i].type == node)
        {
            if (grammar[i].nodes[0] < AST_INVALID &&
                !checked_nodes_contains(terminals, grammar[i].nodes[0]))
            {
                /*
                 * If the symbol is a terminal value and we have not already
                 * added it to terminals then add it
                 */
                list_append(terminals, (void *)grammar[i].nodes[0]);
            }
            else if (grammar[i].nodes[0] != node)
            {
                /*
                 * If the symbol is a non-terminal value then recurse and find
                 * the head terminal
                 */
                head_terminal_values(grammar[i].nodes[0], checked_nodes,
                                     terminals);
            }
        }
    }
}

static int
items_contains(struct listnode **items, struct rule *r, int position, struct listnode *lookahead)
{
    int contains = 0;
    struct listnode *c;
    struct item *i;

    c = *items;
    while (c != NULL)
    {
        i = (struct item *)c->data;
        if (i->rewrite_rule == r && i->cursor_position == position &&
            list_equal(i->lookahead, lookahead))
        {
            contains = 1;
            break;
        }

        c = c->next;
    }
    return contains;
}

/*
 * Generate the items for a given production node.
 */
void
generate_items(enum astnode_t node, struct listnode *lookahead, struct listnode **items)
{
    int i;
    struct item *item;
    struct listnode *checked_nodes, *next_lookahead;

    for (i=0; i<NUM_RULES; i++)
    {
        if (grammar[i].type == node && !items_contains(items, &grammar[i], 0, lookahead))
        {
            item = malloc(sizeof(struct item));
            item->rewrite_rule = &grammar[i];
            item->cursor_position = 0;
            item->lookahead = lookahead;

            list_append(items, item);

            /*
             * Recurse if the derivation begins with variable.
             */
            if (grammar[i].nodes[0] > AST_INVALID)
            {
                if (grammar[i].length_of_nodes > 1)
                {
                    list_init(&checked_nodes);
                    list_init(&next_lookahead);

                    head_terminal_values(
                        grammar[i].nodes[1],
                        &checked_nodes,
                        &next_lookahead);
                    generate_items(grammar[i].nodes[0], next_lookahead, items);
                }
                else
                {
                    generate_items(grammar[i].nodes[0], lookahead, items);
                }
            }
        }
    }
}

/*
 * Generate a state with items, recursively construct connecting states and
 * transitions.
 */
void
generate_transitions(struct state *s)
{
    struct listnode *items;
    struct item *i, *j;
    struct rule *r;
    int index, new_index;


    for (items=s->items; items!=NULL; items=items->next)
    {
        i = (struct item *)items->data;
        r = i->rewrite_rule;

        /*
         * Check if the item contains another consumable value.
         */
        if (i->cursor_position < r->length_of_nodes)
        {
            j = malloc(sizeof(struct item));
            j->rewrite_rule = i->rewrite_rule;

            j->lookahead = i->lookahead;
            j->cursor_position = i->cursor_position + 1;

            index = INDEX(i->rewrite_rule->nodes[i->cursor_position]);
            if (s->links[index] == NULL)
            {
                s->links[index] = &temp_states[tmp_state_identifier++];
                memset(s->links[index], 0, sizeof(struct state));
            }

            if (items_contains(&s->links[index]->items, j->rewrite_rule,
                                j->cursor_position, j->lookahead))
            {
                /*
                 * If state already contains item then continue.
                 */
                free(j);
                continue;
            }

            list_append(&s->links[index]->items, j);

            if (j->cursor_position < j->rewrite_rule->length_of_nodes &&
                j->rewrite_rule->nodes[j->cursor_position] > AST_INVALID)
            {
                /*
                 * Append new states due to subsequent non-terminal.
                 */
                struct listnode *lookahead;
                list_init(&lookahead);

                if (j->cursor_position + 1 < j->rewrite_rule->length_of_nodes)
                {
                    /*
                     * If a follow symbol exists then find terminal values of
                     * the follow symbol. Then generate items derived from the
                     * current symbol and add them to state with the new
                     * lookahead.
                     */
                    struct listnode *checked_nodes;
                    list_init(&checked_nodes);

                    head_terminal_values(
                        j->rewrite_rule->nodes[j->cursor_position + 1],
                        &checked_nodes,
                        &lookahead);

                    generate_items(
                        j->rewrite_rule->nodes[j->cursor_position],
                        lookahead, &s->links[index]->items);
                }
                else
                {
                    /*
                     * If there is no follow symbol then simply generate the
                     * items derived fromthe current symbol and pass along the
                     * current lookahead from the previous state.
                     */
                    generate_items(
                        j->rewrite_rule->nodes[j->cursor_position],
                        j->lookahead, &s->links[index]->items);
                }

            }
        }
    }

    for (index=0; index<NUM_SYMBOLS; index++)
    {
        new_index = index_of_state(s->links[index]);

        if (s->links[index] != NULL && new_index == -1)
        {
            /*
             * If the state does not exist in global states then update the
             * links and recursively generate next transitions. We waited until
             * now to set the link because we finally know all items have been
             * added and can avoid duplicate states.
             */
            new_index = state_identifier++;
            assert(state_identifier < MAX_STATES);

            s->links[index]->identifier = new_index;
            states[new_index] = *s->links[index];

            generate_transitions(&states[new_index]);
        }
        else if (s->links[index] != NULL)
        {
            /*
             * It may be the case that state was recursively generated. In
             * which case we do not want to recurse again, but we still need to
             * set the identifier.
             */
            s->links[index]->identifier = new_index;
        }
    }
}

/*
 * Generates all state for grammar and returns the root state.
 */
struct state *
generate_states(void)
{
    struct state *s;

    memset(states, 0, sizeof(struct state) * MAX_STATES);

    s = &states[0];
    s->identifier = state_identifier++;

    generate_items(AST_TRANSLATION_UNIT, NULL, &s->items);
    generate_transitions(s);

    return s;
}

/*
 * Returns whether the given state contains the given item.
 */
int
state_contains_item(struct state *state, struct item *item)
{
    int contains = 0;
    struct listnode *l;
    struct item *i;

    for (l=state->items; l!=NULL; l=l->next)
    {
        i = (struct item *)l->data;

        if (item->rewrite_rule == i->rewrite_rule &&
            item->cursor_position == i->cursor_position &&
            list_equal(item->lookahead, i->lookahead))
        {
            contains = 1;
            break;
        }
    }
    return contains;
}

int
compare_states(struct state *a, struct state *b)
{
    int index, i, compare;
    struct listnode *l;

    compare = 0;

    if ((a == NULL && b != NULL) || (a != NULL && b == NULL))
    {
        return -1;
    }

    /*
     * Check that indexed state contains everything in state.
     */
    for (l=a->items; l!=NULL; l=l->next)
    {
        if (!state_contains_item(b, (struct item *)l->data))
        {
            compare = -1;
            break;
        }
    }

    /*
     * Inverse check that state contains everything in indexed state.
     */
    for (l=b->items; l!=NULL; l=l->next)
    {
        if (!state_contains_item(a, (struct item *)l->data))
        {
            compare = 1;
            break;
        }
    }

    return compare;
}

/*
 * Returns the index of a state in global states that has identical items or -1
 * if does not exist.
 */
int
index_of_state(struct state *state)
{
    int index, i, match;
    struct listnode *l;

    index = -1;
    for (i=0; i<state_identifier; i++)
    {
        if (compare_states(state, &states[i]) == 0)
        {
            index = i;
            break;
        }
    }

    return index;
}

void
init_parsetable(void)
{
    int i, j;
    struct parsetable_item *row, *cell;
    struct state *state;
    struct listnode *node, *inner_node;
    struct item *item;
    int lookahead;

    if (parsetable != NULL)
    {
        return;
    }

    generate_states();

    parsetable = malloc(sizeof(struct parsetable_item) * (NUM_SYMBOLS) * (state_identifier + 1));
    memset(parsetable, 0, sizeof(struct parsetable_item) * NUM_SYMBOLS * (state_identifier + 1));

    for (i=0; i<state_identifier; i++)
    {
        state = &states[i];
        row = &parsetable[state->identifier * NUM_SYMBOLS];

        for (j=0; j<NUM_SYMBOLS; j++)
        {
            cell = row + j;

            if (state->links[j] != NULL && j > AST_INVALID)
            {
                /*
                 * non-terminal value
                 */
                cell->state = state->links[j]->identifier;
            }
            else if (state->links[j] != NULL && j < AST_INVALID)
            {
                /*
                 * terminal value
                 */
                cell->shift = 1;
                cell->state = state->links[j]->identifier;
            }
        }

        /*
         * Iterate over all items and for each completed rules add to parse
         * table based on lookahead.
         */
        foreach(node, state->items)
        {
            item = ((struct item *)node->data);
            if (item->cursor_position == item->rewrite_rule->length_of_nodes)
            {
                foreach(inner_node, item->lookahead)
                {
                    lookahead = (int)inner_node->data;
                    cell = row + lookahead;

                    cell->reduce = 1;
                    cell->rule = item->rewrite_rule;
                }

                if (item->lookahead == NULL)
                {
                    /*
                     * NULL lookahead means end of input (e.g. $). Since
                     * nothing should match AST_INVALID, let's use that column
                     * for NULL lookahead items.
                     */
                    cell = row + AST_INVALID;

                    cell->reduce = 1;
                    cell->rule = item->rewrite_rule;
                }
            }
        }
    }
}

struct astnode *
token_to_astnode(struct token *token)
{
    struct astnode *node;
    if (token->type == TOK_INTEGER)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_INTEGER_CONSTANT;
        node->constant = token;
    }
    else if (token->type == TOK_IDENTIFIER)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_IDENTIFIER;
        node->constant = token;
    }
    else if (token->type == TOK_PLUS)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_PLUS;
        node->constant = token;
    }
    else if (token->type == TOK_PLUS_PLUS)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_PLUS_PLUS;
        node->constant = token;
    }
    else if (token->type == TOK_PLUS_EQUAL)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_PLUS_EQUAL;
        node->constant = token;
    }
    else if (token->type == TOK_MINUS)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_MINUS;
        node->constant = token;
    }
    else if (token->type == TOK_MINUS_MINUS)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_MINUS_MINUS;
        node->constant = token;
    }
    else if (token->type == TOK_MINUS_EQUAL)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_MINUS_EQUAL;
        node->constant = token;
    }
    else if (token->type == TOK_AMPERSAND)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_AMPERSAND;
        node->constant = token;
    }
    else if (token->type == TOK_AMPERSAND_AMPERSAND)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_AMPERSAND_AMPERSAND;
        node->constant = token;
    }
    else if (token->type == TOK_ASTERISK)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_ASTERISK;
        node->constant = token;
    }
    else if (token->type == TOK_ASTERISK_EQUAL)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_ASTERISK_EQUAL;
        node->constant = token;
    }
    else if (token->type == TOK_BACKSLASH)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_BACKSLASH;
        node->constant = token;
    }
    else if (token->type == TOK_BACKSLASH_EQUAL)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_BACKSLASH_EQUAL;
        node->constant = token;
    }
    else if (token->type == TOK_CARET)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_CARET;
        node->constant = token;
    }
    else if (token->type == TOK_COMMA)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_COMMA;
        node->constant = token;
    }
    else if (token->type == TOK_ELLIPSIS)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_ELLIPSIS;
        node->constant = token;
    }
    else if (token->type == TOK_MOD)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_MOD;
        node->constant = token;
    }
    else if (token->type == TOK_MOD_EQUAL)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_MOD_EQUAL;
        node->constant = token;
    }
    else if (token->type == TOK_QUESTIONMARK)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_QUESTIONMARK;
        node->constant = token;
    }
    else if (token->type == TOK_COLON)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_COLON;
        node->constant = token;
    }
    else if (token->type == TOK_SEMICOLON)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_SEMICOLON;
        node->constant = token;
    }
    else if (token->type == TOK_LPAREN)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_LPAREN;
        node->constant = token;
    }
    else if (token->type == TOK_RPAREN)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_RPAREN;
        node->constant = token;
    }
    else if (token->type == TOK_LBRACKET)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_LBRACKET;
        node->constant = token;
    }
    else if (token->type == TOK_RBRACKET)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_RBRACKET;
        node->constant = token;
    }
    else if (token->type == TOK_LBRACE)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_LBRACE;
        node->constant = token;
    }
    else if (token->type == TOK_RBRACE)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_RBRACE;
        node->constant = token;
    }
    else if (token->type == TOK_VERTICALBAR)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_VERTICALBAR;
        node->constant = token;
    }
    else if (token->type == TOK_VERTICALBAR_VERTICALBAR)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_VERTICALBAR_VERTICALBAR;
        node->constant = token;
    }
    else if (token->type == TOK_SHIFTLEFT)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_SHIFTLEFT;
        node->constant = token;
    }
    else if (token->type == TOK_SHIFTRIGHT)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_SHIFTRIGHT;
        node->constant = token;
    }
    else if (token->type == TOK_LESSTHAN)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_LT;
        node->constant = token;
    }
    else if (token->type == TOK_GREATERTHAN)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_GT;
        node->constant = token;
    }
    else if (token->type == TOK_LESSTHANEQUAL)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_LTEQ;
        node->constant = token;
    }
    else if (token->type == TOK_GREATERTHANEQUAL)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_GTEQ;
        node->constant = token;
    }
    else if (token->type == TOK_EQ)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_EQ;
        node->constant = token;
    }
    else if (token->type == TOK_NEQ)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_NEQ;
        node->constant = token;
    }
    else if (token->type == TOK_EQUAL)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_EQUAL;
        node->constant = token;
    }
    else if (token->type == TOK_VOID)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_VOID;
        node->constant = token;
    }
    else if (token->type == TOK_SHORT)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_SHORT;
        node->constant = token;
    }
    else if (token->type == TOK_INT)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_INT;
        node->constant = token;
    }
    else if (token->type == TOK_CHAR)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_CHAR;
        node->constant = token;
    }
    else if (token->type == TOK_LONG)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_LONG;
        node->constant = token;
    }
    else if (token->type == TOK_FLOAT)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_FLOAT;
        node->constant = token;
    }
    else if (token->type == TOK_DOUBLE)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_DOUBLE;
        node->constant = token;
    }
    else if (token->type == TOK_SIGNED)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_SIGNED;
        node->constant = token;
    }
    else if (token->type == TOK_UNSIGNED)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_UNSIGNED;
        node->constant = token;
    }
    else if (token->type == TOK_AUTO)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_AUTO;
        node->constant = token;
    }
    else if (token->type == TOK_REGISTER)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_REGISTER;
        node->constant = token;
    }
    else if (token->type == TOK_STATIC)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_STATIC;
        node->constant = token;
    }
    else if (token->type == TOK_EXTERN)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_EXTERN;
        node->constant = token;
    }
    else if (token->type == TOK_TYPEDEF)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_TYPEDEF;
        node->constant = token;
    }
    else if (token->type == TOK_GOTO)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_GOTO;
        node->constant = token;
    }
    else if (token->type == TOK_CONTINUE)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_CONTINUE;
        node->constant = token;
    }
    else if (token->type == TOK_BREAK)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_BREAK;
        node->constant = token;
    }
    else if (token->type == TOK_RETURN)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_RETURN;
        node->constant = token;
    }
    else if (token->type == TOK_FOR)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_FOR;
        node->constant = token;
    }
    else if (token->type == TOK_DO)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_DO;
        node->constant = token;
    }
    else if (token->type == TOK_WHILE)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_WHILE;
        node->constant = token;
    }
    else if (token->type == TOK_IF)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_IF;
        node->constant = token;
    }
    else if (token->type == TOK_ELSE)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_ELSE;
        node->constant = token;
    }
    else if (token->type == TOK_SWITCH)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_SWITCH;
        node->constant = token;
    }
    else if (token->type == TOK_CASE)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_CASE;
        node->constant = token;
    }
    else if (token->type == TOK_DEFAULT)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_DEFAULT;
        node->constant = token;
    }
    else if (token->type == TOK_ENUM)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_ENUM;
        node->constant = token;
    }
    else if (token->type == TOK_STRUCT)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_STRUCT;
        node->constant = token;
    }
    else if (token->type == TOK_UNION)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_UNION;
        node->constant = token;
    }
    else if (token->type == TOK_CONST)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_CONST;
        node->constant = token;
    }
    else if (token->type == TOK_VOLATILE)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_VOLATILE;
        node->constant = token;
    }
    else if (token->type == TOK_EOF)
    {
        node = malloc(sizeof(struct astnode));
        node->type = AST_INVALID;
        node->constant = token;
    }

    return node;
}

struct astnode *
parse(struct listnode *tokens)
{
    struct astnode *node, *root;
    struct listnode *stack;
    struct listnode *token;
    struct parsetable_item *row, *cell;
    static int zero = 0;
    int i;

    list_init(&stack);

    /*
     * Stack starts at state 0.
     */
    list_prepend(&stack, &zero);

    for (token=tokens; token!=NULL; )
    {
        row = parsetable + *(int *)stack->data * NUM_SYMBOLS;

        node = token_to_astnode((struct token *)token->data);
        cell = row + INDEX(node->type);
        if (cell->shift)
        {
            /*
             * Shift involves pushing node and state onto stack.
             */
            list_prepend(&stack, node);
            list_prepend(&stack, &cell->state);

            /*
             * Consume a token
             */
            token=token->next;
        }
        else if (cell->reduce)
        {
            root = malloc(sizeof(struct astnode));
            memset(root, 0, sizeof(struct astnode));

            root->type = cell->rule->type;

            /*
             * Reduce involves removing the astnodes that compose the rule from
             * the stack. Then create the reduced astnode and push it onto the
             * stack.
             */
            for (i=0; i<cell->rule->length_of_nodes; i++)
            {
                list_append(&root->children, stack->data);

                /*
                 * Remove astnode and cell state from the stack.
                 */
                stack = stack->next->next;
            }

            /*
             * Push the reduced node and the next state number.
             */
            row = parsetable + *(int *)stack->data * NUM_SYMBOLS;
            cell = row + INDEX(root->type);

            list_prepend(&stack, root);
            list_prepend(&stack, &cell->state);

            /*
             * Next iteration will use the cell->state, but should reuse the
             * current input token. (Do not increment token->next)
             */
        }
        else
        {
            /*
             * We expect to be neither shift nor reduce iff this is the last
             * token.
             */
            assert(token->next == NULL);
            break;
        }
    }

    return root;
}
