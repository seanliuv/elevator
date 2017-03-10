#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "bsem.h"

//------------------------------------------------------------
//structure defines.
typedef struct st_bsem
{
	pthread_mutex_t	mutex;
	pthread_cond_t	cond;
	int v;
}bsem_t;

//------------------------------------------------------------
bsem_t * bsem_create(int value)
{
	if(value != 0 && value != 1)
	{
		printf("bsem_create(): binary semaphore can take only 1 or 0.\n");
		exit(EXIT_FAILURE);
	}

	bsem_t * bsem_p = new bsem_t;
	pthread_mutex_init(&(bsem_p->mutex), NULL);
	pthread_cond_init(&(bsem_p->cond), NULL);
	bsem_p->v = value;
	return bsem_p;
}

void bsem_post(bsem_t * bsem_p)
{
	pthread_mutex_lock(&bsem_p->mutex);
	bsem_p->v = 1;
	pthread_mutex_unlock(&bsem_p->mutex);
	pthread_cond_signal(&bsem_p->cond);
}

void bsem_post_all(bsem_t * bsem_p)
{
	pthread_mutex_lock(&bsem_p->mutex);
	bsem_p->v = 1;
	pthread_mutex_unlock(&bsem_p->mutex);
	pthread_cond_broadcast(&bsem_p->cond);
}

void bsem_wait(bsem_t * bsem_p)
{
	pthread_mutex_lock(&bsem_p->mutex);
	while(bsem_p->v != 1)
	{
		pthread_cond_wait(&bsem_p->cond, &bsem_p->mutex);
	}
	bsem_p->v = 0;
	pthread_mutex_unlock(&bsem_p->mutex);
}

void bsem_reset(bsem_t * bsem_p)
{
	pthread_mutex_init(&(bsem_p->mutex), NULL);
	pthread_cond_init(&(bsem_p->cond), NULL);
	bsem_p->v = 0;
}

void bsem_destroy(bsem_t * bsem_p)
{
	pthread_mutex_destroy(&bsem_p->mutex);
	pthread_cond_destroy(&bsem_p->cond);
	delete bsem_p;
}
