#include "request.h"
#include "server_thread.h"
#include "common.h"
#include <stdbool.h>

struct server {
	int nr_threads;
	int max_requests;
	int max_cache_size;
	int exiting;
	/* add any other parameters you need */
	int *buf;
	int buf_size;
	int in;
	int out;
	pthread_t *threads;
	pthread_cond_t full;
	pthread_cond_t empty;
	pthread_mutex_t lock;
};

/*Hash Table Funcitons from Lab1*/
struct wc* wc; 

//Hashtable element
typedef struct HashNode{
	struct file_data* file;
	int times;
	struct HashNode* next;
}node;

/* free all file data */
static void
file_data_free(struct file_data *data)
{
	free(data->file_name);
	free(data->file_buf);
	free(data);
}

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

//the cache (wc == webserver cache???)
struct wc {
	/* you can define this struct to have whatever fields you want. */
	node** my_table;
	long table_size;
	int max_cache_size;
	int available_cache_size;
	pthread_mutex_t lock;
	node* LRU_list_head;
};

//using LRU to maintain a linked list, put LRU element at the head of list
//in other words, insert new element at tail
void update_replacement_list(struct file_data *file, struct wc* wc){
	node* cur = wc->LRU_list_head;
	node* pre = cur;
	while (cur != NULL){ //check whether the element is already in the list
		if (strcmp(cur->file->file_name, file->file_name) == 0){
			break;
		}
		pre = cur;
		cur = cur->next;
	}
	
	if (cur == NULL){ //the element is not in the list
	////fprintf(stderr, "insert in LRU: %s\n",file->file_name);
		//note: now pre points to the tail
		node* new_tail = malloc(sizeof(node));
		new_tail->file = file;
		new_tail->next = NULL;
		new_tail->times = 1;
		//check if the list is emtpy
		if (pre == NULL){
			wc->LRU_list_head = new_tail;
		} else{
			pre->next = new_tail;
		}
		
	} else{ //the element is in the list, move that to the tail
		//find the tail
		node* tail = cur;
		while (tail->next != NULL){
			tail = tail->next;
		}

		if (tail == cur) //if cur is just the tail
			return;

		if (pre == cur){ //if it is the first element
			wc->LRU_list_head = wc->LRU_list_head->next;
		} else{
			pre->next = cur->next;
		}

		tail->next = cur;
		cur->next = NULL;
	}
	
	return;
}

//find the file in the cache
//if not in the cache, return NULL
struct file_data* table_find(struct file_data *file, struct wc* wc){
	long h = hash(file->file_name);
	if (h < 0) h *= -1;
	long index = h % wc->table_size;

	if (wc->my_table[index] == NULL) return NULL;//if it is not in the table

	node* cur = wc->my_table[index];
	while (cur != NULL){
		if (strcmp(cur->file->file_name, file->file_name) == 0){ //cache hit
			return cur->file;
		}
		cur = cur->next;
	}
	return NULL;
}

//insert a file named word to the hashtable
//if the file is already in the hashtable, return the file
struct file_data* table_insert(struct file_data *file, struct wc* wc){
	long h = hash(file->file_name);
	if (h < 0) h *= -1;
	long index = h % wc->table_size;

	if (wc->my_table[index] == NULL){ //if the list is emtpy
		wc->my_table[index] = (node*)malloc(sizeof(node));
		wc->my_table[index]->file = file;
		wc->my_table[index]->times = 1;
		wc->my_table[index]->next = NULL;
		wc->available_cache_size -= file->file_size;
	} else{
		//insert at head
		node* temp = (node*)malloc(sizeof(node));
		temp->file = file;
		temp->times = 1;
		temp->next = wc->my_table[index];
		wc->my_table[index] = temp;
		wc->available_cache_size -= file->file_size;
	}
	////fprintf(stderr, "Insert in Cache: %s\n",file->file_name);
	return NULL;
}

//evict nodes in the hashtable to make enough space
void cache_evict(int required_size, struct wc* wc){
	while (wc->available_cache_size < required_size){
		node* target = wc->LRU_list_head; //target is at the head of the LRU list 
		wc->LRU_list_head = wc->LRU_list_head->next;
		wc->available_cache_size += target->file->file_size;
		////fprintf(stderr, "Delete in LRU: %s\n",target->file->file_name);
		long h = hash(target->file->file_name);
		if (h < 0) h *= -1;
		long index = h % wc->table_size;

		node* cur = wc->my_table[index];
		node* pre = cur;
		////fprintf(stderr, "1.5\n");
		//if the list to the table entry only has one element
		//if (cur == NULL) //fprintf(stderr, "yes\n");
		if (cur->next == NULL){ //not sure whether free() will set the head pointer to NULL
			wc->my_table[index] = NULL;
			////fprintf(stderr, "1.6\n");
			goto free;
		}
		////fprintf(stderr, "2\n");
		//delete the node
		while (cur->file != target->file){
			pre = cur;
			cur = cur->next;
		}
		////fprintf(stderr, "3\n");
		pre->next = cur->next;
		////fprintf(stderr, "4\n");

free:	////fprintf(stderr, "Delete in Cache: %s\n",cur->file->file_name);
		//file_data_free(cur->file);
		free(cur);
		free(target);
		
	}
}

//insert file into the cache, ignore files that are too big
int cache_insert(struct file_data *file, struct wc *wc){
	if (file->file_size >= wc->max_cache_size){ //if the file is too big
		return -1;
	} 

	//make space for the new element (if already have enrough space, this function just return)
	////fprintf(stderr, "haha1\n");
	if (wc->available_cache_size < file->file_size){
		cache_evict(file->file_size, wc);
	}
	////fprintf(stderr, "haha2\n");
	table_insert(file, wc);
	////fprintf(stderr, "haha3\n");
	update_replacement_list(file, wc);
	return 0;
}

//destroy the hashtable
void
wc_destroy(struct wc *wc)
{
	//free the hashtable
	for (int i = 0; i < wc->table_size; i++){
		if (wc->my_table[i] != NULL){
			node* cur = wc->my_table[i];
			while (cur != NULL){
				node* temp = cur;
				cur = cur->next;
				file_data_free(temp->file);
				free(temp);
			}
		}
	}
	free(wc->my_table);

	//free the LRU list
	node* cur = wc->LRU_list_head;
	while (cur != NULL){
		node* temp = cur;
		cur = cur->next;
		free(temp);
	}


	free(wc);
}


/* static functions */

/* initialize file data */
static struct file_data *
file_data_init(void)
{
	struct file_data *data;

	data = Malloc(sizeof(struct file_data));
	data->file_name = NULL;
	data->file_buf = NULL;
	data->file_size = 0;
	return data;
}

static void
do_server_request(struct server *sv, int connfd)
{
	int ret;
	int isInsert = 1;
	struct request *rq;
	struct file_data *data;

	data = file_data_init();

	/* fill data->file_name with name of the file being requested */
	rq = request_init(connfd, data);
	if (!rq) {
		file_data_free(data);
		return;
	}

	if (sv->max_cache_size > 0){
		//fprintf(stderr, "here1\n");
		struct file_data* cached_file = NULL;
		pthread_mutex_lock(&wc->lock);
		//fprintf(stderr, "here1.5\n");
		cached_file = table_find(data, wc);
		//fprintf(stderr, "here2\n");
		pthread_mutex_unlock(&wc->lock);

		if (cached_file != NULL){ //cache hit
			//fprintf(stderr, "here3\n");
			pthread_mutex_lock(&wc->lock);
			update_replacement_list(data, wc);
			//fprintf(stderr, "here4\n");
			pthread_mutex_unlock(&wc->lock);

			request_set_data(rq, cached_file);
			goto send;
		}
		//fprintf(stderr, "here5\n");
	}
	//fprintf(stderr, "here5.1\n");

	/* read file, 
	 * fills data->file_buf with the file contents,
	 * data->file_size with file size. */
	ret = request_readfile(rq);
	if (ret == 0) { /* couldn't read file */
		//fprintf(stderr, "here5.2\n");
		goto out;
	}

	if (sv->max_cache_size > 0){
		//fprintf(stderr, "here5.3\n");
		pthread_mutex_lock(&wc->lock);
		//fprintf(stderr, "here6\n");
		if (table_find(data, wc) == NULL){ //in case other thread accessing the same file
			//fprintf(stderr, "here7\n");
			isInsert = cache_insert(data, wc);
			//fprintf(stderr, "here8\n");
		}
		pthread_mutex_unlock(&wc->lock);
	}

	/* send file to client */
send:	request_sendfile(rq);
out:
	request_destroy(rq);
	if (sv->max_cache_size == 0 || isInsert == -1){
		file_data_free(data);
	}
	
}

/* entry point functions */

void* stub_receiver(void* input){
	struct server *sv = input;
	while(true){ //need the loop since threads should keep dealing with requests
		pthread_mutex_lock(&sv->lock);
		while (sv->in == sv->out){
			pthread_cond_wait(&sv->empty, &sv->lock);
			if (sv->exiting){
				pthread_mutex_unlock(&sv->lock); //remember to unlock
				return NULL;
			} 
		} //empty
		int sockfd = sv->buf[sv->out];
		sv->out = (sv->out + 1) % sv->buf_size;
		pthread_cond_signal(&sv->full);
		pthread_mutex_unlock(&sv->lock);
		do_server_request(sv, sockfd); //Can do outside of critical section
	}
	return NULL;
}

struct server *
server_init(int nr_threads, int max_requests, int max_cache_size)
{
	struct server *sv;

	sv = Malloc(sizeof(struct server));
	sv->nr_threads = nr_threads;
	sv->max_requests = max_requests;
	sv->max_cache_size = max_cache_size;
	sv->exiting = 0;
	
	if (nr_threads > 0 || max_requests > 0 || max_cache_size > 0) {
		/* Lab 4: create queue of max_request size when max_requests > 0 */
		pthread_cond_init(&sv->full, NULL);
		pthread_cond_init(&sv->empty, NULL);
		pthread_mutex_init(&sv->lock, NULL);
		sv->buf = (int*)malloc(sizeof(int) * (max_requests + 1)); //+1 to distinguish between empty and full
		sv->buf_size = max_requests + 1;
		sv->threads = (pthread_t*)malloc(sizeof(pthread_t) * nr_threads);
		sv->in = 0;
		sv->out = 0;
		////fprintf(stderr, "here2\n");
		/* Lab 5: init server cache and limit its size to max_cache_size */
		if (max_cache_size > 0){
			wc = malloc(sizeof(struct wc));
			wc->table_size = 1024 * 32; //arbitrary big size
			wc->my_table = malloc(sizeof(node*) * wc->table_size);
			////fprintf(stderr, "here2.5\n");
			for (int i = 0; i < wc->table_size; ++i){
				wc->my_table[i] = NULL;
			}
			////fprintf(stderr, "here2.6\n");
			wc->LRU_list_head = NULL;
			wc->max_cache_size = max_cache_size;
			wc->available_cache_size = wc->max_cache_size;
			pthread_mutex_init(&wc->lock, NULL);
			////fprintf(stderr, "here3\n");
		}

		/* Lab 4: create worker threads when nr_threads > 0 */
		//fprintf(stderr, "#thread: %d\n", nr_threads);
		for (int i = 0; i < nr_threads; ++i){
			pthread_create(&sv->threads[i], NULL, stub_receiver, sv);
		}
	}
	
	//printf("here\n");
	return sv;
}

void
server_request(struct server *sv, int connfd)
{
	if (sv->nr_threads == 0) { /* no worker threads */
		do_server_request(sv, connfd);
	} else {
		/*  Save the relevant info in a buffer and have one of the
		 *  worker threads do the work. */
		pthread_mutex_lock(&sv->lock);
		while ((sv->in - sv->out + sv->buf_size) % sv->buf_size == sv->buf_size - 1){
			pthread_cond_wait(&sv->full, &sv->lock);
			if (sv->exiting){
				pthread_mutex_unlock(&sv->lock);
				return;
			} 
		} //full
		sv->buf[sv->in] = connfd;
		sv->in = (sv->in + 1) % sv->buf_size;
		pthread_cond_signal(&sv->empty);
		pthread_mutex_unlock(&sv->lock);
	}
}

void
server_exit(struct server *sv)
{
	/* when using one or more worker threads, use sv->exiting to indicate to
	 * these threads that the server is exiting. make sure to call
	 * pthread_join in this function so that the main server thread waits
	 * for all the worker threads to exit before exiting. */
	sv->exiting = 1;
	pthread_cond_broadcast(&sv->full);
	pthread_cond_broadcast(&sv->empty);

	for (int i = 0; i < sv->nr_threads; ++i){
		assert(pthread_join(sv->threads[i], NULL) == 0);
	}
	

	/* make sure to free any allocated resources */
	
	free(sv->buf);
	free(sv->threads);
	free(sv);
}
