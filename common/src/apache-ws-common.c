#include <apache-ws-common.h>

#include <stdio.h>
#include <stdbool.h>

#if defined (_WIN32)
#define false 0
#define true 1
#pragma comment(lib, "Ws2_32.lib")
#include <Ws2tcpip.h>
#define unlink _unlink
#else
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <poll.h>

#include <sys/socket.h>
#include <sys/un.h>

#define sizeof_sockaddr_un(x) (sizeof(x) - sizeof(x.sun_path) + strlen(x.sun_path))
#define closesocket(x) do {shutdown((x), SHUT_RDWR); close((x));} while(0)
typedef int SOCKET;
typedef int DWORD;
#endif

struct apachews_stream {
    uint8_t *data;
    size_t length;
    size_t size;
};

#define DEFAULT_CLIENTS_COUNT 1024
#define BASE_BUFFER_SIZE 0x4000

struct apachews_event {
    apachews_client *client;
    apachews_event *next;
    apachews_event_type type;
    const apachews_context *context;
};

struct apachews_client {
    apachews_context *context;
    SOCKET sock;
    char *language;
};

typedef struct apachews_client_list {
    apachews_client *client;
    struct apachews_client_list *next;
} apachews_client_list;

struct apachews_context {
    SOCKET server;
    apachews_client_list *clients;

    struct pollfd *pollfds;
    int pollcnt;
    apachews_event *queue;
};

static int
apachews_clientcmp(const void *const _lhs, const void *const _rhs)
{
    const apachews_client *lhs;
    const apachews_client *rhs;

    lhs = _lhs;
    rhs = _rhs;

    return (int) (lhs->sock - rhs->sock);
}

apachews_client_list *
apachews_client_list_create_node(apachews_client *const value)
{
    apachews_client_list *list;

    list = malloc(sizeof(*list));
    if (list == NULL)
        return NULL;
    list->client = value;
    list->next = NULL;

    return list;
}

apachews_client *
apachews_client_list_get_client(apachews_client_list *const list)
{
    return list->client;
}

static int
apachews_poll(struct pollfd **set, int count, SOCKET sock, short events)
{
    struct pollfd *added;
    if ((*set == NULL) || (count >= DEFAULT_CLIENTS_COUNT)) {
        size_t blocks;
        void *buffer;
        blocks = 1 + count / DEFAULT_CLIENTS_COUNT;
        buffer = realloc(*set, blocks * DEFAULT_CLIENTS_COUNT * sizeof (**set));
        if (buffer == NULL)
            return -1;
        *set = buffer;
    }

    if (*set == NULL)
        return -1;

    added = &((*set)[count]);

    added->fd = sock;
    added->revents = 0;
    added->events = events;

    return count + 1;
}

static int
apachews_find_pollfd_position(int count, const struct pollfd *const set, SOCKET which)
{
    for (int index = 0 ; index < count; ++index) {
        const struct pollfd *item;
        item = &set[index];
        if (item->fd != which)
            continue;
        return index;
    }
    return -1;
}

static void
apachews_unpoll(apachews_context *const ctx, SOCKET sock)
{
    int index;
    index = apachews_find_pollfd_position(ctx->pollcnt, ctx->pollfds, sock);
    if (index != -1) {
        struct pollfd *start;
        ssize_t length;
        // Subtract the removed descriptor
        ctx->pollcnt -= 1;
        // How many items need to be moved?
        length = ctx->pollcnt - index;
        // Point to the object that we will replace
        start = ctx->pollfds + index;
        // Finally move the data
        memmove(start, start + 1, length);
    }
}

bool
apachews_client_list_append(apachews_context *const ctx, apachews_client *const value)
{
    apachews_client_list *node;
    apachews_client_list *list;
    SOCKET sock;
    ssize_t count;
    sock = apachews_client_get_socket(value);
    // Attempt to add the socket to the
    // poll set.
    count = apachews_poll(&ctx->pollfds, ctx->pollcnt, sock, POLLIN);
    if (count == -1)
        return false;
    // Added successfuly, update the
    // counter
    ctx->pollcnt += 1;
    // Insert the `client' object into the
    // list now
    list = ctx->clients;
    node = apachews_client_list_create_node(value);
    if (node == NULL)
        goto error;
    node->next = list->next;
    list->next = node;

    return true;
error:
    apachews_unpoll(ctx, apachews_client_get_socket(value));
    return false;
}

apachews_client_list **
apachews_client_list_find(apachews_client_list *list, const apachews_client *const value)
{
    for (apachews_client_list **node = &list; *node != NULL; node = &(*node)->next) {
        if (apachews_clientcmp(value, (*node)->client) == 0) {
            return node;
        }
    }
    return NULL;
}

apachews_client *
apachews_create_client(SOCKET sock, apachews_context *const ctx)
{
    apachews_client *client;
    client = malloc(sizeof(*client));
    if (client == NULL)
        return NULL;
    client->context = ctx;
    client->sock = sock;
    // TODO: add more data perhaps?
    return client;
}

static apachews_context *
apachews_create_context(SOCKET sock)
{
    apachews_context *ctx;
    apachews_client *root;
    short events;
    // Allocate space
    ctx = malloc(sizeof(*ctx));
    if (ctx == NULL)
        return NULL;
    root = apachews_create_client(INVALID_SOCKET, ctx);
    if (root == NULL)
        goto error;
    // Make initial room for the clients
    events = POLLIN | POLLHUP;
    // Insert the server into the poll set to
    // receive accept and probably close events
    // too
    ctx->pollcnt = apachews_poll(&ctx->pollfds, ctx->pollcnt, sock, events);
    if (ctx->pollcnt == -1)
        goto error;
    ctx->clients = apachews_client_list_create_node(root);
    // Populate
    ctx->queue = NULL;
    ctx->server = sock;    
    return ctx;
error:
    apachews_client_free(root);
    free(ctx);
    return NULL;
}

#if defined (_WIN32)
static int
apachews_get_last_port()
{
    FILE *file;
    int value;
    // Open the ports list file
    if (fopen_s(&file, ".ws-ports", "r") != 0)
        return 15100; // The default value
    // Move to the last entry
    fseek(file, sizeof(short int), SEEK_END);
    // Read the last entry
    if (fread(&value, sizeof(short int), 1, file) != 1) {
        // Cleanup
        fclose(file);
        // Return the default value
        return 15100;
    }
    // Cleanup
    fclose(file);
    // Return the last value
    return value + 1;
}

static void
apachews_update_last_port(int current)
{
    FILE *file;
    int value;
    // Open the file for appending
    if (fopen_s(&file, ".ws-ports", "a") != 0)
        return;
    value = current;
    // Write thew new value to the file
    fwrite(&value, sizeof(short int), 1, file);
    // Clean up
    fclose(file);
}

static BOOL
apachews_create_uds(const char *const path, int port)
{
    FILE *file;
    // Create a file to perform IPC as if it was
    // a unix domain socket
    if (fopen_s(&file, path, "w") != 0)
        return false;
    // Write the port number to it so it can be used
    // by the other end to connect to us
    if (fwrite(&port, sizeof(int), 1, file) != 1) {
        // Clean up
        fclose(file);
        return false;
    }
    // Clean up
    fclose(file);
    return true;
}

apachews_context *
apachews_create(const char *const path)
{
    int port;
    struct sockaddr_in address;
    u_long enable;
    SOCKET sock;
    // Initialize all variables
    sock = INVALID_SOCKET;
    // Get the value, of the last available port number
    port = apachews_get_last_port();
    // Create a socket
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET)
        goto error;
    // Make the socket NON-Blocking
    enable = true;
    if (ioctlsocket(sock, FIONBIO, &enable) != 0)
        goto error;
    // Bind this socket to the local loopback device
    // strictly, and use the appropriate port
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    // Copy the IP address
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
    // Bind to this address
    if (bind(sock, (struct sockaddr *) &address, (socklen_t) sizeof(address)) == SOCKET_ERROR)
        goto error;
    // Listen on this socket
    if (listen(sock, 1024) != 0)
        goto error;
    // Fake UDS on windows OS
    if (apachews_create_uds(path, port) == false)
        goto error;
    // Create the extension execution context
    return apachews_create_context(sock);
error:
    if (sock != INVALID_SOCKET)
        closesocket(sock);
    return NULL;
}
#else

apachews_context *
apachews_create(const char *const path)
{
    socklen_t length;
    struct sockaddr_un address;
    struct sigaction signals;
    int flags;
    int sock;

    signals.sa_flags = 0;
    signals.sa_handler = SIG_IGN;

    sigaction(SIGUSR1, &signals, NULL);
    // Create a Unix Domain Socket
    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock == -1)
        return NULL;
    // Make the socket Non-Blocking
    flags = fcntl(sock, F_GETFL, 0);
    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1)
        goto error;
    // Populate the socket address
    address.sun_family = AF_UNIX;
    // FIXME: we should check here for buffer overflow
    strcpy(address.sun_path, path);
    // Get the length of the socket
    length = sizeof_sockaddr_un(address);
    // Bind the socket to the local filesystem address
    if (bind(sock, (struct sockaddr *) &address, length) == -1)
        goto error;
    // Start listening on the socket
    if (listen(sock, 1024) == -1)
        goto error;
    // Create the execution context
    return apachews_create_context(sock);
error:
    close(sock);
    return NULL;
}
#endif

void
apachews_context_free(apachews_context *ctx)
{
    if (ctx == NULL)
        return;
    if (ctx->server != -1)
        close(ctx->server);
    // FIXME: remove them all?
    free(ctx->clients);
    free(ctx->queue);
    free(ctx);
}

void
apachews_context_remove_client(apachews_context *ctx, const apachews_client *const client)
{
    apachews_client_list **found;
    apachews_unpoll(ctx, apachews_client_get_socket(client));
    // Relink the linked list and free the element
    found = apachews_client_list_find(ctx->clients, client);
    if (*found != NULL) {
        apachews_client_list *next;
        next = (*found)->next;
        free(*found);
        *found = next;
    }
}

BOOL
apachews_queue_event(apachews_event **queue, apachews_client *const client, apachews_context *ctx, apachews_event_type type)
{
    apachews_event *event;
    // Create the new event, that will be queued into the
    // event queue
    event = malloc(sizeof(*event));
    if (event == NULL)
        return false;
    // Point to the current head in the linked list
    // and populate the event
    event->next = *queue;
    event->client = client;
    event->type = type;
    event->context = ctx;
    // Reset the head to this new event
    *queue = event;
    // Tell the caller everything was fine
    return true;
}

static apachews_event *
apachews_dequeue_event(apachews_context *ctx)
{
    apachews_event *event;
    // Save a pointer to the current head of the linked list
    event = ctx->queue;
    if (event == NULL)
        return NULL;
    // Make the next node the current head
    ctx->queue = event->next;
    return event;
}

apachews_event *
apachews_create_close_event()
{
    apachews_event *event;
    // Attempt to allocate memory
    event = malloc(sizeof(*event));
    if (event == NULL)
        return NULL;
    // Populate the event
    event->next = NULL;
    // This allows to have a reference to the
    // connected client
    event->client = NULL;
    // This, is mandatory
    event->type = ApacheWSCloseEvent;
    event->context = NULL;
    return event;
}

apachews_event *
apachews_create_accept_event(apachews_context *ctx, apachews_client *const client)
{
    apachews_event *event;
    // Attempt to allocate memory
    event = malloc(sizeof(*event));
    if (event == NULL)
        return NULL;
    // Populate the event
    event->next = NULL;
    // This allows to have a reference to the
    // connected client
    event->client = client;
    // This, is mandatory
    event->type = ApacheWSAcceptEvent;
    // Always keep the extension IN CONTEXT
    event->context = ctx;
    return event;
}

const apachews_context *
apachews_event_get_context(const apachews_event * const event)
{
    return event->context;
}

apachews_event_type
apachews_event_get_type(const apachews_event * const event)
{
    return event->type;
}

const char *
apachews_client_get_language(const apachews_client *const ctx)
{
    return ctx->language;
}

apachews_client *
apachews_find_client_by_socket(apachews_client_list *list, SOCKET sock)
{
    while (list != NULL) {
        apachews_client *client;
        client = list->client;
        if (client == NULL)
            continue; // WTF?
        if (client->sock == sock)
            return client;
        list = list->next;
    }
    return NULL;
}

static bool
apachews_is_error_event(int events)
{
    return
        ((events & POLLERR ) == POLLERR ) ||
        ((events & POLLNVAL) == POLLNVAL)
    ;
}

static apachews_event *
apachews_server_get_event(const struct pollfd *const set, apachews_context *const ctx)
{
    apachews_event *event;
    if (apachews_is_error_event(set->revents) == true) {
        event = apachews_create_close_event();
        if (set->fd != ctx->server) {
            apachews_client *client;
            client = apachews_find_client_by_socket(ctx->clients, set->fd);
            if (client != NULL) {
                apachews_context_remove_client(ctx, client);
            }
        }
        apachews_unpoll(ctx, set->fd);
    } if (set->fd == ctx->server) {
        apachews_client *client;
        SOCKET sock;
        // It's an accept event
        //
        // If the event occurred with the server descriptor, then it
        // means that `accept()' would return immediately.
        //
        // So, we will call `accept()' and add the returned socket to
        // the list of client sockets. And then, return the event so
        // the caller can continue with the next event
        sock = accept(ctx->server, NULL, NULL);
        if (sock == INVALID_SOCKET)
            return NULL;
        client = apachews_create_client(sock, ctx);
        // Append the accepted client to the list
        if (apachews_client_list_append(ctx, client) == false)
            return NULL;
        // Make a accept event, this is created on the heap because
        // PHP will deallocate the memory so using a stack variable
        // for this is not possible.
        event = apachews_create_accept_event(ctx, client);
    } else {
        // Check if there are read events
        apachews_client *client;
        client = apachews_find_client_by_socket(ctx->clients, set->fd);
        if (client != NULL) {
            // Add the potential event to the queue
            apachews_queue_event(&ctx->queue, client, ctx, ApacheWSIOEvent);
        }
        event = NULL;
    }
    return event;
}

apachews_event *
apachews_server_next_event(apachews_context *ctx)
{
    // fd_set rfds;
    // SOCKET nfds;
    apachews_event *event;
    apachews_event *queue;
    int count;

    if (ctx == NULL)
        return NULL;
    // Make a pointer to the `queue' of events
    queue = ctx->queue;
    // If there are events in the queue, dequeue them
    if ((queue != NULL) && (queue->next != NULL))
        goto dequeue;
    // Perform descriptor selection
    if ((count = poll(ctx->pollfds, ctx->pollcnt, -1)) == SOCKET_ERROR)
        return NULL;
    for (int index = 0; index < ctx->pollcnt; ++index) {
        const struct pollfd *pfd;
        pfd = &ctx->pollfds[index];
        if (pfd->revents == 0)
            continue;
        event = apachews_server_get_event(pfd, ctx);
        if (event != NULL) {
            // If an event is returned, return it immediately
            // because it's an `accept()' generated in the server
            // socket
            return event;
        }
    }
dequeue:
    return apachews_dequeue_event(ctx);
}

void
apachews_event_free(apachews_event *event)
{
    if (event == NULL)
        return;
    free(event);
}

apachews_stream *
apachews_stream_new(void)
{
    apachews_stream *stream;
    stream = malloc(sizeof(*stream));
    if (stream == NULL)
        return NULL;
    stream->data = malloc(BASE_BUFFER_SIZE);
    if (stream->data == NULL) {
        free(stream);
        return NULL;
    }
    stream->size = BASE_BUFFER_SIZE;
    stream->length = 0;

    return stream;
}

void
apachews_stream_free(apachews_stream *stream)
{
    if (stream == NULL)
        return;
    free(stream->data);
    free(stream);
}

int
apachews_stream_resize(apachews_stream *stream, size_t hint)
{
    size_t size;
    void *data;
    size = hint - hint % BASE_BUFFER_SIZE + BASE_BUFFER_SIZE;
    data = realloc(stream->data, size);
    if (data == NULL)
        return -1;
    stream->data = data;
    stream->size = size;
    return 0;
}

int
apachews_stream_append(apachews_stream *stream, const char *const what, size_t length)
{
    size_t size;
    size = stream->length + length;
    if ((size > stream->size) && (apachews_stream_resize(stream, size) == -1))
        return -1;
    memcpy(stream->data + stream->length, what, length);
    stream->length += length;
    return 0;
}

uint8_t *
apachews_stream_data(const apachews_stream *const buffer)
{
    return buffer->data;
}

size_t
apachews_stream_length(const apachews_stream *const stream)
{
    return stream->length;
}


#if defined (_WIN32)
void
apachews_initialize_os(void)
{
    WSADATA data;
    _unlink(".ws-ports");
    WSAStartup(MAKEWORD(2, 2), &data);
}
#else
void apachews_initialize_os(void) {}
#endif

static int
apachews_get_error(void)
{
#if defined (_WIN32)
    DWORD _errno;
    _errno = WSAGetLastError();
    switch (_errno) {
    case 0:
        return ApacheWSSuccess;
    case WSAEWOULDBLOCK:
        return ApacheWSWouldBlock;
    default:
        return ApacheWSOther;
    }
#else
    switch (errno) {
    case 0:
        return ApacheWSSuccess;
    case EWOULDBLOCK:
        return ApacheWSWouldBlock;
    default:
        return ApacheWSOther;
    }
#endif
    return ApacheWSOther;
}

apachews_status
apachews_event_read(const apachews_event *const event, uint8_t **data, size_t *length)
{
    SOCKET sock;
    apachews_stream *stream;
    ssize_t size;
    char buffer[1024];

    // Build a stream to read all the data in it
    // this could be a problem if the data size
    // exceedes available memory, but that is simply
    // a non realistic scenario.
    stream = apachews_stream_new();
    if (stream == NULL)
        return ApacheWSOutOfMemory;
    // Extract the socket from the event
    sock = apachews_client_get_socket(event->client);
    if (sock == INVALID_SOCKET)
        goto error;
    // Start reading data until
    //
    // 1. There's no more data (errno == EWOULDBLOCK || EAGAIN)
    // 2. An error occurs
    // 3. The connection is closed
    while ((size = recv(sock, buffer, sizeof(buffer) - 1, MSG_DONTWAIT)) > 0)
        apachews_stream_append(stream, buffer, size);
    switch (size) {
    case SOCKET_DISCONNECTED: // This means the connection was closed
        apachews_stream_free(stream);
        return ApacheWSConnectionClosed;
    case SOCKET_ERROR: // Any possible error
        // These two values do not mean an error really
        // they mean that there is no more data, or that
        // to read more data we need to go again or the
        // call would block which has the same meaning
        if (apachews_get_error() != ApacheWSWouldBlock)
            goto error;
    default:
        // Build a python 'bytes' value to return it
        *length = (ssize_t) apachews_stream_length(stream);
        *data = apachews_stream_data(stream);
        // Release temoporary memory
        free(stream);
        break;
    }
    return ApacheWSSuccess;
error:
    // Release temporary data
    apachews_stream_free(stream);
    // This is a real error, not an exception
    // it has to be handled in the python code
    return ApacheWSError;
}

static ssize_t
apachews_write(SOCKET sock, const uint8_t *data, size_t length)
{
    ssize_t remaining;
    remaining = (ssize_t) length;
    // Write everything, until there is no more data
    while (remaining > 0) {
        ssize_t result;
        result = send(sock, data, (int) remaining, MSG_NOSIGNAL);
        if (result == -1) // If an error occurred send it back
            return -1;
        data += result;
        remaining -= result;
    }
    return length - remaining;
}

ssize_t
apachews_event_respond(const apachews_event *const event, uint8_t *data, size_t length)
{    
    SOCKET sock;
    // Extract the socket from the pointer
    sock = apachews_client_get_socket(event->client);
    if (sock == INVALID_SOCKET)
        return -1;
    // Return the length of delivered data
    return apachews_write(sock, data, length);
}

ssize_t
apachews_server_broadcast(const apachews_context *const ctx, const uint8_t *const data, size_t length)
{
    ssize_t total;
    total = 0;
    for (apachews_client_list *node = ctx->clients; node != NULL; node = node->next)
        total += apachews_client_send(apachews_client_list_get_client(node), data, length);
    return total;
}

ssize_t
apachews_client_send(const apachews_client *const client, const void *const data, size_t length)
{
    return apachews_write(apachews_client_get_socket(client), data, length);
}

int
apachews_server_close(apachews_context *const ctx)
{
    // Close it now
    closesocket(ctx->server);
    // Raise the signal now
    raise(SIGUSR1);
    return 0;
}

int
apachews_client_close(apachews_client *const client)
{
    if (client == NULL)
        return -1;
    closesocket(client->sock);
    // Remove the client from the context
    apachews_context_remove_client(client->context, client);
    return 0;
}

apachews_client *
apachews_event_get_client(const apachews_event *const event)
{
    return event->client;
}

SOCKET
apachews_client_get_socket(const apachews_client *const client)
{
    return client->sock;
}

void
apachews_client_free(apachews_client *const client)
{
    free(client);
}
