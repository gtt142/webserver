#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <err.h>
#include <event2/buffer.h>
#include <event2/event.h>
#include <event2/bufferevent.h>

#include "parse_config.h"
#include "config.h"
#include "workqueue.h"
#include "responce_parse.h"
#include "http_head.h"

/**
 * Struct to carry around connection (client)-specific data.
 */
typedef struct client {
	/* The client's socket. */
	int fd;

	/* The event_base for this client. */
	struct event_base *evbase;

	/* The bufferedevent for this client. */
    struct bufferevent *buf_ev;

	/* The output buffer for this client. */
	struct evbuffer *output_buffer;
} client_t;

char root_dir[128];

static struct event_base *evbase_accept;
static workqueue_t workqueue;

/* Signal handler function (defined below). */
static void sighandler(int signal);

/**
 * Set a socket to non-blocking mode.
 */
static int setnonblock(int fd) {
	int flags;

	flags = fcntl(fd, F_GETFL);
	if (flags < 0) 
		return flags;
	flags |= O_NONBLOCK;
	if (fcntl(fd, F_SETFL, flags) < 0) 
		return -1;
	return 0;
}

static void closeClient(client_t *client) {
	if (client != NULL) {
		if (client->fd >= 0) {
			close(client->fd);
			client->fd = -1;
		}
	}
}

static void closeAndFreeClient(client_t *client) {
	if (client != NULL) {
		closeClient(client);
		// if (client->buf_ev != NULL) {
		// 	bufferevent_free(client->buf_ev);
		// 	client->buf_ev = NULL;
		// }
		if (client->evbase != NULL) {
			event_base_free(client->evbase);
			client->evbase = NULL;
		}
		if (client->output_buffer != NULL) {
			evbuffer_free(client->output_buffer);
			client->output_buffer = NULL;
		}
		free(client);
	}
}

/**
 * Called by libevent when there is data to read.
 */
void buffered_on_read(struct bufferevent *bev, void *arg) {

    printf("START BUFFEREVENT READING\n");

	client_t *client = (client_t *)arg;
	http_t http_request;
    int response_id;

    char response[RESPONSE_BUFSIZE];
    printf("BEFORE HTTP PARSE\n");

    response_id = http_parse(&http_request, bufferevent_get_input(bev), root_dir);
    printf("AFTER HTTP PARSE\n");
    switch (response_id) {
        case NOT_ALLOWED_HTTP_METHOD :
            create_response("405", "Not Implemented", response);
            printf("405 ERROR\n");
            break;
        case FILE_NOT_EXIST :
            create_response("404", "Not found", response);
            printf("404 ERROR\n");
            break;
        case FILE_IS_EXECUTABLE :
            create_response("500", "Internal server error", response);
            printf("500 ERROR\n");
            break;
        case ALL_OK :
            create_response("200", "OK", response);
            printf("200 OK\n");
            break;
        case ESCAPING_ROOT :
            printf("403 ERROR\n");
            create_response("403", "Forbidden", response);
            break;
        case PARSE_ERROR : 
            printf("PARSE ERROR\n");
        	create_response("400", "Bad request", response);
            break;
        case INDEX_FILE_NOT_EXIST:
            printf("INDEX ERROR\n");
            create_response("403", "Forbidden", response);
            break;
        default:
            printf("DEFAULT 500 ERROR\n");
            create_response("500", "Internal server error", response);
            break;
    }

    printf("BEFORE CONTENT-LENGTH\n");

    if (response_id == ALL_OK) {
        sprintf(response + strlen(response), "Content-Length: %lu\r\n", http_request.filesize);
        sprintf(response + strlen(response), "Content-Type: %s\r\n\r\n", http_request.filetype);
    }

    evbuffer_add(client->output_buffer, response, strlen(response));
    printf("CLIENT OUTPUT BUFFER FILLED WITH RESPONSE\n");
    if (strcmp(http_request.method, "GET") == 0 && response_id == ALL_OK) {
        printf("INSIGHT GET 200 HEADER CONDITION\n");
        int fd = open(http_request.filename, O_RDONLY, 0);
        evbuffer_add_file(client->output_buffer, fd, 0, http_request.filesize);
        printf("CLIENT OUTPUT BUFFER FILLED WITH FILE WITH SIZE %zu\n", http_request.filesize);
    }
    if(strcmp(http_request.method, "HEAD") == 0){
        printf("THIS IS HEAD\n%s", response);
    }
    bufferevent_disable(bev, EV_READ);
    bufferevent_enable(bev, EV_WRITE);
	/* Send the results to the client.  This actually only queues the results for sending.
	 * Sending will occur asynchronously, handled by libevent. */
	if (bufferevent_write_buffer(bev, client->output_buffer) != 0) {
        printf("BUFFEREVENT ERROR\n");
        closeClient(client);
    }

    //struct timeval delay = { 1e };
}

void buffered_on_write(struct bufferevent *bev, void *arg) {
    struct evbuffer *output = bufferevent_get_output(bev);
    if (evbuffer_get_length(output) == 0) {
        printf("flushed answer\n");
        bufferevent_free(bev);
    }
}

void eventcb(struct bufferevent *bev, short events, void *arg)
{
    if (events & BEV_EVENT_EOF) {
        printf("Connection closed.\n");
    } else if (events & (BEV_EVENT_ERROR)) {
         perror("Error on the connection");
         //closeClient((client_t *)arg);
         bufferevent_free(bev);
         //event_base_loopexit(client->evbase, NULL);
    }
    //bufferevent_free(bev);
}


static void server_job_function(struct job *job) {
    printf("WORKER START DOING JOB\n");
	client_t *client = (client_t *)job->user_data;

	// struct timeval timer = {0, 10000};
	// bufferevent_set_timeouts(client->buf_ev, &timer, &timer);	
	event_base_dispatch(client->evbase);
    printf("WORKER AFTER EVENT BASE LOOP\n");
	closeAndFreeClient(client);
    printf("CLIENT CLOSED\n");
	free(job);
}

/**
 * This function will be called by libevent when there is a connection
 * ready to be accepted.
 */
void on_accept(int fd, short ev, void *arg) {

    printf("ACCEPT CONNECTION\n");
	int client_fd;
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);
	workqueue_t *workqueue = (workqueue_t *)arg;
	client_t *client;
	job_t *job;

	client_fd = accept(fd, (struct sockaddr *)&client_addr, &client_len);
	if (client_fd < 0) {
		warn("accept failed");
		return;
	}

	/* Set the client socket to non-blocking mode. */
	if (setnonblock(client_fd) < 0) {
		warn("failed to set client socket to non-blocking");
		close(client_fd);
		return;
	}

	/* Create a client object. */
	if ((client = malloc(sizeof(*client))) == NULL) {
		warn("failed to allocate memory for client state");
		close(client_fd);
		return;
	}

	memset(client, 0, sizeof(*client));
	client->fd = client_fd;

	if ((client->output_buffer = evbuffer_new()) == NULL) {
		warn("client output buffer allocation failed");
		closeAndFreeClient(client);
		return;
	}

	if ((client->evbase = event_base_new()) == NULL) {
		warn("client event_base creation failed");
		closeAndFreeClient(client);
		return;
	}

    printf("BEFORE SOCKET NEW\n");

	/* Create the buffered event.*/
    if ((client->buf_ev = bufferevent_socket_new(client->evbase, client->fd, BEV_OPT_CLOSE_ON_FREE )) == NULL) {
        warn("client bufferevent creation failed");
        closeAndFreeClient(client);
        return;
    }

    printf("AFTER SOCKET NEW\n");

    bufferevent_setcb(client->buf_ev, buffered_on_read, buffered_on_write, eventcb, client);

    printf("AFTER SET CALLBACK\n");

	/* We have to enable it before our callbacks will be
	 * called. */
	if(bufferevent_enable(client->buf_ev, EV_READ)){
        warn("error in bufferevent for client");
        return;
    };

    printf("AFTER ENABLE\n");


	/* Create a job object and add it to the work queue. */
	if ((job = malloc(sizeof(*job))) == NULL) {
		warn("failed to allocate memory for job state");
		closeAndFreeClient(client);
		return;
	}

    printf("AFTER ALLOCATE JOB\n");
    job->job_function = server_job_function;
	job->user_data = client;

    printf("BEFORE ADD JOB\n");

	workqueue_add_job(workqueue, job);

    printf("AFTER ADD JOB\n");

}

/**
 * Run the server.  This function blocks, only returning when the server has terminated.
 */
int start(void) {
	struct configf config;
    config.port = DEFAULT_SERVER_PORT;
    config.cpu = DEFAULT_NUM_THREADS;

    if (sprintf(config.path, "%s", DEFAULT_DOCUMENT_ROOT) < 0) {
        perror("Sprintf error");
    }

    int error = parse_config(&config);
    if (error != 0) {
        fprintf(stderr, "Can't parse %s\n", PATH);
        //return error;
    }
    printf("path -- %s\n", config.path);

    strcpy(root_dir, config.path);

	int sin;
	struct sockaddr_in s_addr;

	/* Set signal handlers */
	sigset_t sigset;
	sigemptyset(&sigset);

    struct sigaction siginfo = {
		.sa_handler = sighandler,
		.sa_mask = sigset,
		.sa_flags = SA_RESTART,
	};

	sigaction(SIGINT, &siginfo, NULL);
	sigaction(SIGTERM, &siginfo, NULL);

	/* Create our listening socket. */
	sin = socket(AF_INET, SOCK_STREAM, 0);
	if (sin < 0) {
		err(1, "listen failed");
	}

	setsockopt(sin, SOL_SOCKET, SO_REUSEADDR, &s_addr, sizeof(s_addr));

	memset(&s_addr, 0, sizeof(s_addr));
	s_addr.sin_family = AF_INET;
	s_addr.sin_addr.s_addr = INADDR_ANY;
	s_addr.sin_port = htons(config.port);

    if (bind(sin, (struct sockaddr *)&s_addr, sizeof(s_addr)) < 0) {
		err(1, "bind failed");
	}

    if (listen(sin, CONNECTION_BACKLOG) < 0) {
		err(1, "listen failed");
	}


	/* Set the socket to non-blocking, this is essential in event
	 * based programming with libevent. */
	if (setnonblock(sin) < 0) {
		perror("failed to set server socket to non-blocking");
	}

	if ((evbase_accept = event_base_new()) == NULL) {
		perror("Unable to create socket accept event base");
		close(sin);
		return 1;
	}

	/* Initialize work queue. */
	if (workqueue_init(&workqueue, config.cpu)) {
		perror("Failed to create work queue");
		close(sin);
		workqueue_shutdown(&workqueue);
		return 1;
	}

	/* We now have a listening socket, we create a read event to
	 * be notified when a client connects. */
    event_add(event_new(evbase_accept, sin, EV_READ|EV_PERSIST, on_accept, (void *)&workqueue), NULL);


	printf("Server running.\n");

	/* Start the event loop. */
	event_base_dispatch(evbase_accept);

	event_base_free(evbase_accept);
	evbase_accept = NULL;

	close(sin);

	printf("Server shutdown.\n");

	return 0;
}

/**
 * Kill the server.  This function can be called from another thread to kill the
 * server, causing start() to return.
 */
void killServer(void) {
	fprintf(stdout, "Stopping socket listener event loop.\n");
	if (event_base_loopexit(evbase_accept, NULL)) {
		perror("Error shutting down server");
	}
	fprintf(stdout, "Stopping workers.\n");
	workqueue_shutdown(&workqueue);
}

static void sighandler(int signal) {
	fprintf(stdout, "Received signal %d: %s.  Shutting down.\n", signal, strsignal(signal));
	killServer();
}

int main(int argc, char *argv[]) {
	return start();
}

