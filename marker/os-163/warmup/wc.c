#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "common.h"
#include "wc.h"
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

typedef struct HashNode{
	char* word;
	int times;
	struct HashNode* next;
}node;

/*A Hash Function from http://www.cse.yorku.ca/~oz/hash.html */
unsigned long
    hash(char *str)
    {
        unsigned long hash = 5381;
        int c;

        while ((c = *str++) != 0)
            hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
		
        return hash;
    }

struct wc {
	/* you can define this struct to have whatever fields you want. */
	node** my_table;
	long table_size;
};

void table_insert(char* word, struct wc* wc){
	long h = hash(word);
	if (h < 0) h *= -1;
	long index = h % wc->table_size;

	if (wc->my_table[index] == NULL){
		wc->my_table[index] = (node*)malloc(sizeof(node));
		wc->my_table[index]->word = (char*)malloc(sizeof(char) * (strlen(word) + 1));
		strcpy(wc->my_table[index]->word, word);
		wc->my_table[index]->times = 1;
		wc->my_table[index]->next = NULL;
	} else{
		node* cur = wc->my_table[index];
		while (cur != NULL){
			if (strcmp(cur->word, word) == 0){
				cur->times++;
				return;
			}
			cur = cur->next;
		}
		node* temp = (node*)malloc(sizeof(node));
		temp->word = (char*)malloc(sizeof(char) * (strlen(word) + 1));
		strcpy(temp->word, word);
		temp->times = 1;
		temp->next = wc->my_table[index];
		wc->my_table[index] = temp;
	}
}

void table_init(char* word_array, long size, struct wc *wc){
	int startID = 0;
	for (int i = 0; word_array[i] != '\0'; ++i){
		if (isspace(word_array[i])){

			int length = i - startID;
			char* word = (char*)malloc(sizeof(char) * (length + 1));
			strncat(word, &word_array[startID], length);
			word[length] = '\0';

			if (strcmp(word,"\0") != 0)
				table_insert(word, wc);

			free(word);
			startID = i + 1;
		}
	}
}

struct wc *
wc_init(char *word_array, long size)
{

	struct wc *wc;

	wc = (struct wc *)malloc(sizeof(struct wc));
	assert(wc);

	wc->table_size = size;
	wc->my_table = (node**)malloc(sizeof(node*) * wc->table_size);
	assert(wc->my_table);
	
	table_init(word_array, size, wc);

	return wc;
}

void
wc_output(struct wc *wc)
{
	for (int i = 0; i < wc->table_size; ++i){
		if (wc->my_table[i] != NULL){
			node* cur = wc->my_table[i];
			while (cur != NULL){
				printf("%s:%d\n", cur->word, cur->times);
				cur = cur->next;
			}
		}
	}
}

void
wc_destroy(struct wc *wc)
{
	for (int i = 0; i < wc->table_size; i++){
		if (wc->my_table[i] != NULL){
			node* cur = wc->my_table[i];
			while (cur != NULL){
				node* temp = cur;
				cur = cur->next;
				free(temp->word);
				free(temp);
			}
		}
	}
	free(wc->my_table);
	free(wc);
}
