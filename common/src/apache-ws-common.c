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

#include <sys/socket.h>
#include <sys/un.h>

#define sizeof_sockaddr_un(x) (sizeof(x) - sizeof(x.sun_path) + strlen(x.sun_path))
#define closesocket(x) do {shutdown((x), SHUT_RDWR); close((x));} while(0)
typedef int SOCKET;
typedef int DWORD;
#endif

typedef struct apachews_stream {
    uint8_t *data;
    size_t length;
    size_t size;
} apachews_stream;

#define DEFAULT_CLIENTS_COUNT 1024
#define BASE_BUFFER_SIZE 0x4000

typedef struct apachews_event {
    SOCKET socket;
    apachews_event *next;
    apachews_event_type type;
    apachews_context *context;
} apachews_event;

typedef struct apachews_context {
    SOCKET server;
    SOCKET *clients;
    apachews_event *queue;
    size_t count;
    size_t size;
} apachews_context;

static apachews_context *
apachews_create_context(SOCKET sock)
{
    apachews_context *context;
    // Allocate space
    context = malloc(sizeof(*context));
    if (context == NULL)
        return NULL;
    // Populate
    context->queue = NULL;
    context->server = sock;
    // Make initial room for the clients
    context->clients = malloc(DEFAULT_CLIENTS_COUNT * sizeof(*context->clients));
    // Check that there is initial room
    if (context->clients == NULL) {
        context->size = 0;
    } else {
        context->size = DEFAULT_CLIENTS_COUNT;
    }
    context->count = 0;
    return context;
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
    int flags;
    int sock;
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
apachews_context_free(apachews_context *context)
{
    if (context == NULL)
        return;
    free(context->clients);
    free(context->queue);
    free(context);
}

static int
apachews_sockcmp(const void *const _lhs, const void *const _rhs)
{
    const SOCKET *lhs;
    const SOCKET *rhs;

    lhs = _lhs;
    rhs = _rhs;

    return (int) (*lhs - *rhs);
}

void
apachews_context_remove_client(apachews_context *ctx, SOCKET sock)
{
    SOCKET *client;
    client = bsearch(&sock, ctx->clients, ctx->count, sizeof(sock), apachews_sockcmp);
    if (client == NULL)
        return;
    *client = ctx->clients[ctx->count - 1];

    qsort(ctx->clients, ctx->count--, sizeof(sock), apachews_sockcmp);
}

BOOL
apachews_context_append_client(apachews_context *context, SOCKET client)
{
    // Check if we have room for the next client
    if (context->count + 1 > context->size) {
        SOCKET *clients;
        size_t size;
        // Increase the size to make room for the base default count
        // again
        size = context->size + DEFAULT_CLIENTS_COUNT;
        // Reallocate memory for the SOCKET objects
        clients = realloc(context->clients, size * sizeof(*context->clients));
        if (clients == NULL)
            return false;
        // Update the context structure now that everything
        // went OK
        context->clients = clients;
        context->size = size;
    }
    // Insert the new element into the list
    context->clients[context->count++] = client;
    // Sort items
    qsort(context->clients, context->count,
          sizeof(*context->clients), apachews_sockcmp);
    return true;
}

BOOL
apachews_queue_event(apachews_event **queue,
    SOCKET socket, apachews_context *context, apachews_event_type type)
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
    event->socket = socket;
    event->type = type;
    event->context = context;
    // Reset the head to this new event
    *queue = event;
    // Tell the caller everything was fine
    return true;
}

static apachews_event *
apachews_dequeue_event(apachews_context *context)
{
    apachews_event *event;
    // Save a pointer to the current head of the linked list
    event = context->queue;
    if (event == NULL)
        return NULL;
    // Make the next node the current head
    context->queue = event->next;
    return event;
}

apachews_event *
apachews_create_accept_event(apachews_context *context, SOCKET sock)
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
    event->socket = sock;
    // This, is mandatory
    event->type = ApacheWSAcceptEvent;
    // Always keep the extension IN CONTEXT
    event->context = context;
    return event;
}

SOCKET
apachews_event_get_socket(const apachews_event *const event)
{
    return event->socket;
}

apachews_context *
apachews_event_get_context(const apachews_event * const event)
{
    return event->context;
}

apachews_event_type
apachews_event_get_type(const apachews_event * const event)
{
    return event->type;
}

apachews_event *
apachews_next_event(apachews_context *ctx)
{
    fd_set rfds;
    SOCKET nfds;
    apachews_event *queue;
    int count;
    if (ctx == NULL)
        return NULL;
    // Make a pointer to the `queue' of events
    queue = ctx->queue;
    // If there are events in the queue, dequeue them
    if ((queue != NULL) && (queue->next != NULL))
        goto dequeue;
    // Initialize the select context data
    FD_ZERO(&rfds);
    // Add the server (for accept only)
    FD_SET(ctx->server, &rfds);
    // Store server as `nfds'
    nfds = ctx->server;
    // Add every file descriptor to the set
    for (size_t idx = 0; idx < ctx->count; ++idx) {
        // This is for non-windows only
        if (nfds < ctx->clients[idx])
            nfds = ctx->clients[idx];
        FD_SET(ctx->clients[idx], &rfds);
    }
    // Perform descriptor selection
    if ((count = select((int) nfds + 1, &rfds, NULL, NULL, NULL)) == SOCKET_ERROR)
        return NULL;
    // Check if it's an accept event
    //
    // If the event occurred with the server descriptor, then it
    // means that `accept()' would return immediately.
    //
    // So, we will call `accept()' and add the returned socket to
    // the list of client sockets. And then, return the event so
    // the caller can continue with the next event
    if (FD_ISSET(ctx->server, &rfds) != 0) {
        apachews_event *event;
        SOCKET client;
        client = accept(ctx->server, NULL, NULL);
        if (client == INVALID_SOCKET)
            return NULL;
        // Append the accepted client to the list
        if (apachews_context_append_client(ctx, client) == false)
            return NULL;
        // Make a accept event, this is created on the heap because
        // PHP will deallocate the memory so using a stack variable
        // for this is not possible.
        event = apachews_create_accept_event(ctx, client);
        if (event == NULL)
            return NULL;
        return event;
    } else {
        // Check if there are read events
        for (size_t idx = 0; idx < ctx->count; ++idx) {
            SOCKET client;
            // This is just to avoid repeating code
            client = ctx->clients[idx];
            // Check whether this socket was in the set
            if (FD_ISSET(client, &rfds) == 0)
                continue;
            // Add the socket to the queue
            apachews_queue_event(&ctx->queue, client, ctx, ApacheWSIOEvent);
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
    sock = event->socket;
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
apachews_event_write(const apachews_event *const event, uint8_t *data, size_t length)
{    
    SOCKET sock;
    // Extract the socket from the pointer
    sock = event->socket;
    if (sock == INVALID_SOCKET)
        return -1;
    // Return the length of delivered data
    return apachews_write(sock, data, length);
}

int
apachews_broadcast(const apachews_context *const context, const uint8_t *const data, size_t length)
{
    ssize_t total;
    total = 0;
    for (size_t idx = 0 ; idx < context->count ; ++idx) {
        ssize_t result;
        result = apachews_write(context->clients[idx], data, length);
        if (result == -1)
            goto error;
        total += result;
    }
    return total + 1;
error:
    return -1;
}
