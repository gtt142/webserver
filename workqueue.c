#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "workqueue.h"

#define LINKED_LIST_ADD(item, list) { \
	item->prev = NULL; \
	item->next = list; \
	list = item; \
}

#define LINKED_LIST_REMOVE(item, list) { \
	if (item->prev != NULL) item->prev->next = item->next; \
	if (item->next != NULL) item->next->prev = item->prev; \
	if (list == item) list = item->next; \
	item->prev = item->next = NULL; \
}

static void *worker_function(void *ptr) {
	worker_t *worker = (worker_t *)ptr;
	job_t *job;
	printf("worker function started\n");

	while (1) {
		printf("waiting to be notified\n");
		pthread_mutex_lock(&worker->workqueue->jobs_mutex);
		printf("thread ready\n");
		while (worker->workqueue->waiting_jobs == NULL) {
			if (worker->terminate) break;
			printf("condition wait\n");
			pthread_cond_wait(&worker->workqueue->jobs_cond, &worker->workqueue->jobs_mutex);
		}

		if (worker->terminate) {
			pthread_mutex_unlock(&worker->workqueue->jobs_mutex);
			break;
		}

		job = worker->workqueue->waiting_jobs;
		if (job != NULL) {
			printf("job taken\n");
			LINKED_LIST_REMOVE(job, worker->workqueue->waiting_jobs);
		}
		pthread_mutex_unlock(&worker->workqueue->jobs_mutex);
		usleep(500);

		if (job == NULL) continue;

		printf("job started\n");
		job->job_function(job);
	}

	free(worker);
	pthread_exit(NULL);
}

int workqueue_init(workqueue_t *workqueue, int numWorkers) {
	int i;
	worker_t *worker;
	pthread_cond_t blank_cond = PTHREAD_COND_INITIALIZER;
	pthread_mutex_t blank_mutex = PTHREAD_MUTEX_INITIALIZER;

	if (numWorkers < 1) numWorkers = 1;
	memset(workqueue, 0, sizeof(*workqueue));
	workqueue->jobs_cond = blank_cond;
	workqueue->jobs_mutex = blank_mutex;
	memcpy(&workqueue->jobs_mutex, &blank_mutex, sizeof(workqueue->jobs_mutex));
	memcpy(&workqueue->jobs_cond, &blank_cond, sizeof(workqueue->jobs_cond));

	for (i = 0; i < numWorkers; i++) {
		if ((worker = malloc(sizeof(worker_t))) == NULL) {
			perror("Failed to allocate all workers");
			return 1;
		}
		memset(worker, 0, sizeof(*worker));
		worker->workqueue = workqueue;
		if (pthread_create(&worker->thread, NULL, worker_function, (void *)worker)) {
			perror("Failed to start all worker threads");
			free(worker);
			return 1;
		}
		LINKED_LIST_ADD(worker, worker->workqueue->workers);
	}

	return 0;
}

void workqueue_shutdown(workqueue_t *workqueue) {
	worker_t *worker = NULL;

	/* Set all workers to terminate. */
	for (worker = workqueue->workers; worker != NULL; worker = worker->next) {
		worker->terminate = 1;
	}

	/* Remove all workers and jobs from the work queue.
	 * wake up all workers so that they will terminate. */
	pthread_mutex_lock(&workqueue->jobs_mutex);
	workqueue->workers = NULL;
	workqueue->waiting_jobs = NULL;
	pthread_cond_broadcast(&workqueue->jobs_cond);
	pthread_mutex_unlock(&workqueue->jobs_mutex);
}

void workqueue_add_job(workqueue_t *workqueue, job_t *job) {
	/* Add the job to the job queue, and notify a worker. */
	pthread_mutex_lock(&workqueue->jobs_mutex);
	LINKED_LIST_ADD(job, workqueue->waiting_jobs);
	pthread_cond_signal(&workqueue->jobs_cond);
	pthread_mutex_unlock(&workqueue->jobs_mutex);
}
