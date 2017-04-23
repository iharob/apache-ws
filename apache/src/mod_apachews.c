#include <apache-ws-common.h>

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

#if defined (_WIN32)
#include <winsock2.h>
#include <WS2tcpip.h>

typedef enum bool {
	false, true
} bool;

#pragma comment(lib, "Ws2_32.lib")

typedef long long int ssize_t;
#else
#include <sys/socket.h>
#include <sys/un.h>

#include <endian.h>

#include <stdbool.h>
#include <unistd.h>
#include <dlfcn.h>

#include <stdbool.h>
#include <syslog.h>

#include <poll.h>

#endif

#include <httpd.h>
#include <http_core.h>
#include <http_protocol.h>
#include <http_request.h>
#include <http_log.h>
#include <apr_strings.h>
#include <apr_base64.h>
#include <apr_sha1.h>

#define APACHEWS_MAGIC "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define POLLFD_INITIALIZER {pool, APR_POLL_SOCKET, APR_POLLIN, 0, {NULL}, NULL}
#define countof(x) (sizeof(x) / sizeof(*x))

#define err(...) ap_log_error(__FILE__, __LINE__, apachews_module.module_index, APLOG_CRIT, APR_SUCCESS, NULL, __VA_ARGS__)

#define BUFFER_SIZE 0x4000

static void apachews_register_hooks(apr_pool_t *pool);

enum http_apachews_frame_type {
    WSContinuationFrame = 0x00,
    WSTextFrame = 0x01,
    WSBinaryFrame = 0x02,
    WSCloseFrame = 0x08,
    WSPingFrame = 0x09,
    WSPongFrame = 0x0A,
    WSInvalidFrame
};

struct apachews_frame {
    uint8_t *data;
    enum http_apachews_frame_type type;
    int64_t length;
};

struct apachews_frame_header {
    uint8_t final;
    uint8_t reserved;
    uint8_t type;
    uint8_t masked;
    uint64_t length;
};

static void *apachews_create_configuration(apr_pool_t *p, char *dir);
static const command_rec apachews_directives[] = {
    {NULL}
};

module AP_MODULE_DECLARE_DATA apachews_module = {
    STANDARD20_MODULE_STUFF,
    apachews_create_configuration,
    NULL,
    NULL,
    NULL,
    apachews_directives,
    apachews_register_hooks /* Our hook registering function */
};
#define BASE_BUFFER_SIZE 0x4000

int
apachews_is_big_endian()
{
    union {
        uint32_t integer;
        char data[sizeof(uint32_t)];
    } value = {0x01000000};
    return value.data[0];
}

#if defined (_WIN32)
#define apachews_random rand
#else
uint64_t
ntohll(uint64_t value)
{
	if (apachews_is_big_endian() != 0)
		return value;
	return __bswap_64(value);
}

uint64_t
htonll(uint64_t value)
{
	if (apachews_is_big_endian() != 0)
		return value;
	return __bswap_64(value);
}

static int32_t
apachews_random(void)
{
    int32_t value;
    const char *devices[2] = {"/dev/random", "/dev/urandom"};
    int fd;

    fd = -1;
    for (size_t i = 0 ; i < sizeof(devices) / sizeof(*devices) ; ++i) {
        struct pollfd fds;
        if (fd != -1)
            close(fd);
        if ((fd = open(devices[i], O_RDONLY)) == -1)
            continue;
        fds.events = POLLIN;
        fds.revents = 0;
        fds.fd = fd;
        if ((poll(&fds, 1, 0) == 1) && ((fds.revents & POLLIN) != 0)) {
            if (read(fd, &value, sizeof(value)) != (ssize_t) sizeof(value))
                continue;
            close(fd);
            return value;
        }
    }
    if (fd != -1)
        close(fd);
    return rand();
}
#endif

static bool
apachews_send_string(apr_socket_t *sock, const char *const message, bool masked)
{
    ssize_t length;
    char *frame;
    char *pointer;
    int32_t mask;
    // Sanity checks always good, in case `*message == '\0'` it's an empty
    // message so it makes no sense to send it.
    if ((sock == NULL) || (message == NULL) || (*message == '\0'))
        return false;
    // Get the length of the original mesasge. Ideally, use a `printf' like
    // method to construct a message.
    length = strlen(message);
    // Allocate necessary space for a frame. We allocate the MAXIMUM
    frame = malloc(length + sizeof(int64_t) + sizeof(int16_t) + 2);
    if (frame == NULL)
        return false;
    pointer = frame;
    // Set the first byte. The most significant bit has to be 1 indicating
    // that it's a final frame, we will only send one frame. The least
    // significant bit is also 1, indicating that it's a text frame
    *(pointer++) = 0x81;
    if (length < 0x7E) {
        uint8_t size;
        // Ensure this is an 8 bit value
        size = (uint8_t) (length & 0xFF);
        // This is the message length
        *pointer = size | (masked ? 0x80 : 0x00);
        // Advance the pointer, to continue building the frame
        pointer += 1;
    } else if (length <= 0xFFFF) {
        uint16_t size;
        // Set the value and increase the poitner
        *(pointer++) = 0x7E | (masked ? 0x80 : 0x00);
        // Ensure only 16 bits are assigned
        size = htons((uint16_t) length);
        // Copy the bytes to the frame
        memcpy(pointer, &size, sizeof(size));
        // Advance the pointer, to continue building the frame
        pointer += sizeof(size);
    } else {
        uint64_t size;
        // Set the value and increase the poitner
        *(pointer++) = 0x7F | (masked ? 0x80 : 0x00);
        // Copy the whole value, in any case `size_t` is smaller
        // than int64_t.
        // FIXME: this should be `htonll'
        size = htonll((uint64_t) length);
        // Copy the bytes to the frame
        memcpy(pointer, &size, sizeof(size));
        // Advance the pointer, to continue building the frame
        pointer += sizeof(size);
    }
    // Check if we have to mask the frame the mask is a 32bit integer
    // and the data is masked by taking the xor of the original value
    // with the idx % 4 th byte of the mask.
    if (masked == true) {
        // Generate a mask from a 32bit random number
        mask = apachews_random();
        // Copy this mask to the frame
        memcpy(pointer, &mask, sizeof(mask));
        // Move the pointer to copy the data now
        pointer += sizeof(mask);
        // Copy each masked byte to the frame
        for (size_t idx = 0 ; message[idx] != '\0' ; ++idx) {
            *(pointer++) = message[idx] ^ ((uint8_t *) &mask)[idx % 4];
        }
    } else {
        // No mask required, so copy the raw message to
        // the frame.
        memcpy(pointer, message, length);
    }
    // Compute the actual length of the message
    length = length + (ptrdiff_t) (pointer - frame);
    apr_socket_timeout_set(sock, -1);
    // Send it to the socket
    pointer = frame;
    while (length > 0) {
        apr_status_t status;
        size_t sent;
        sent = length;
        status = apr_socket_send(sock, pointer, &sent);
        if (status != APR_SUCCESS)
            break;
        pointer += sent;
        length -= sent;
    }
    apr_socket_timeout_set(sock, 0);
    // Free temporary memory
    free(frame);
    return (length == 0);
}

static void
apachews_accept_key(const char *const key, char *result)
{
    size_t length;
    uint8_t sha1[APR_SHA1_DIGESTSIZE];
    apr_sha1_ctx_t context;
    char composed[60];

    length = strlen(key);

    memcpy(composed, key, length);
    memcpy(composed + length, APACHEWS_MAGIC, sizeof(APACHEWS_MAGIC));

    apr_sha1_init(&context);
    apr_sha1_update(&context, composed, ((unsigned int) (length + sizeof(APACHEWS_MAGIC) - 1)));
    apr_sha1_final(sha1, &context);

    apr_base64_encode(result, (const char *) sha1, sizeof(sha1));
}

static uint64_t
apachews_get_message_length(const uint8_t *data, size_t *offset)
{
    uint64_t length;
    // Minimal offset is 2
    *offset = 2;
    switch (data[0] & 0x7F) {
    case 0: // This is an error!
        return 0;
    case 126:
        // Copy the raw bytes (Big Endian)
        memcpy(&length, data + 1, sizeof(uint16_t));
        // Update the offset value
        *offset += sizeof(uint16_t);
        // Convert to host endian
        return ntohs((uint16_t) length);
    case 127:
        // Copy the raw bytes (Big Endian)
        memcpy(&length, data + 1, sizeof(length));
        // Update the offset value
        *offset += sizeof(uint64_t);
        // Convert to host endian
        return ntohll((uint64_t) length);
    default:
        // It's a 7 bits value
        return data[0] & 0x7F;
    }
    return 0;
}

static void
apachews_read_text_frame(struct apachews_frame *frame, const uint8_t *data, size_t length)
{
    const uint8_t *payload;
    // Allocate space for the data
    frame->data = malloc(length + 1);
    if (frame->data == NULL)
        return;
    // Save the frame length
    frame->length = length;
    // Payload comes after the masking bytes (4 bytes)
    payload = data + sizeof(int32_t);
    // Frames from clients are masked, unmask it
    for (size_t i = 0 ; i < length ; ++i) {
        frame->data[i] = (int32_t) payload[i] ^ (int32_t) data[i % 4];
    }
    frame->data[length] = '\0';
}

static const uint8_t *
apachews_get_header(const uint8_t *const data, struct apachews_frame_header *header)
{
    size_t offset;
    // Construct the frame header
    header->final = ((data[0] & 0x80) == 0x80);
    header->type = data[0] & 0x0F;
    header->masked = ((data[1] & 0x80) == 0x80);
    header->length = apachews_get_message_length(&data[1], &offset);
    // Return a pointer to the next
    // interesting data in the raw
    // frame
    return data + offset;
}

static void
apachews_frame_get_data(const uint8_t *const data,
                         struct apachews_frame *frame, struct apachews_frame_header *header)
{
    switch (header->type) {
    case WSTextFrame:
        // If it's a frame from the client IT MUST BE MASKED
        if (header->masked == true) {
            apachews_read_text_frame(frame, data, header->length);
        }
        break;
    case WSCloseFrame:
        break;
    default:
        break;
    }
}

static struct apachews_frame *
apachews_get_frame(const uint8_t *const data)
{
    struct apachews_frame_header header;
    struct apachews_frame *frame;
    const uint8_t *pointer;
    // Allocate space for the frame
    frame = malloc(sizeof(*frame));
    if (frame == NULL)
        return NULL;
    // Get the frame header with a helper funcion
    pointer = apachews_get_header(data, &header);
    // Build the full frame
    frame->type = header.type;
    frame->length = 0;
    frame->data = NULL;
    // Finally get the actual data.
    apachews_frame_get_data(pointer, frame, &header);
    return frame;
}

static void *
apachews_create_configuration(apr_pool_t *pool, char *dir)
{
#if defined (_WIN32)
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
        return NULL;
#endif
    return NULL;
}

#if defined (_WIN32)
static int
apachews_get_uds_port(const char *const path)
{
    FILE *file;
    int port;
    if (fopen_s(&file, path, "r") != 0)
        return -1;
    if (fread(&port, sizeof(int), 1, file) != 1) {
        fclose(file);
        return -1;
    }
    fclose(file);
    return port;
}

static apr_socket_t *
apachews_connect_to_data_server(apr_pool_t *pool, const char *const path)
{
    SOCKET ossock;
    struct sockaddr_in address;
    apr_socket_t *sock;
    apr_os_sock_info_t sockinfo;
    apr_status_t status;
    int port;

    if ((port = apachews_get_uds_port(path)) == -1)
        return NULL;
    ossock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ossock == INVALID_SOCKET)
        return NULL;
    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
    if (connect(ossock, (struct sockaddr *) &address,
        (socklen_t) sizeof(address)) == SOCKET_ERROR) {
        goto error;
    }
    // Ensure this in case of returning without
    // initializing
    sock = NULL;
    // Fill the socket information for apache to make us
    // a platform independent APR Socket.
    sockinfo.family = AF_UNIX;
    // This is because we already connected
    sockinfo.remote = (struct sockaddr *) &address;
    // This is for listening sockets
    sockinfo.local = NULL;
    // This is the operating system specific socket
    sockinfo.os_sock = &ossock;
    // Atuomatic
    sockinfo.protocol = 0;
    // Of course SOCK_STREAM just as our os dependant socket
    sockinfo.type = SOCK_STREAM;
    // Try to make it now
    if ((status = apr_os_sock_make(&sock, &sockinfo, pool)) != APR_SUCCESS)
        goto error;
    return sock;
error:
    closesocket(ossock);
    return NULL;
}
#else
static socklen_t
apachews_create_unix_socket_address(struct sockaddr_un *sa, const char *const path)
{
    size_t length;
    length = strlen(path);
    // Set family to AF_UNIX
    // TODO: Support all sorts of sockets
    sa->sun_family = AF_UNIX;
    // Copy the path string
    memcpy(sa->sun_path, path, length + 1);
    // Return the length of the sa
    return length + sizeof(sa->sun_family);
}

static apr_socket_t *
apachews_connect_to_data_server(apr_pool_t *pool, const char *const path)
{
    int fd;
    apr_socket_t *sock;
    apr_os_sock_info_t sockinfo;
    apr_status_t status;
    struct sockaddr_un sa;
    socklen_t length;

    length = apachews_create_unix_socket_address(&sa, path);
    // Make a new Unix Domain Socket
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1)
        goto error;
    // Connect to the Unix Domain Socket to comunnicate
    // with the server program
    if (connect(fd, (struct sockaddr *) &sa, length) == -1)
        goto error;
    // Ensure this in case of returning without
    // initializing
    sock = NULL;
    // Fill the socket information for apache to make us
    // a platform independent APR Socket.
    sockinfo.family = AF_UNIX;
    // This is because we already connected
    sockinfo.remote = (struct sockaddr *) &sa;
    // This is for listening sockets
    sockinfo.local = NULL;
    // This is the operating system specific socket
    sockinfo.os_sock = &fd;
    // Atuomatic
    sockinfo.protocol = 0;
    // Of course SOCK_STREAM just as our os dependant socket
    sockinfo.type = SOCK_STREAM;
    // Try to make it now
    if ((status = apr_os_sock_make(&sock, &sockinfo, pool)) != APR_SUCCESS)
        goto error;
    return sock;
error:
    if (errno != 0)
        err("%s:%d: %s", __FUNCTION__, errno, strerror(errno));
    if (fd != -1)
        close(fd);
    return NULL;
}
#endif

static bool
apachews_read_from_client(apr_pool_t *pool, apr_socket_t *client, apr_socket_t *server)
{
    apr_status_t status;
    char buffer[BUFFER_SIZE];
    const uint8_t *data;
    struct apachews_frame *frame;
    apachews_stream *stream;
    apr_pollset_t *pset;
    apr_pollfd_t pollfd[] = {POLLFD_INITIALIZER};
    size_t length;
    int count;
    bool reading;
    if (apr_pollset_create(&pset, 1, pool, APR_POLLSET_DEFAULT) != APR_SUCCESS)
        return false;
    pollfd[0].desc.s = client;
    // Add the file descriptor to the set
    apr_pollset_add(pset, &pollfd[0]);
    // Create a stream to read the data into
    stream = apachews_stream_new();
    if (stream == NULL)
        return false;
    // Set the initial value of the flag
    reading = true;
    // Set the initial value of the length
    length = sizeof(buffer);
    // This value is one, because there is only one socket
    // to read from
    count = 1;
    while (reading == true) {
        reading = (apr_pollset_poll(pset, count, &count, 0) == APR_SUCCESS);
        if (reading == false)
            continue;
        status = apr_socket_recv(client, buffer, &length);
        switch (status) {
        case APR_EAGAIN:
            break;
        case APR_SUCCESS:
            if (length == 0) {
                reading = false;
            } else if (apachews_stream_append(stream, buffer, length) == -1) {
                apachews_stream_free(stream);
                return false;
            }
            // Reset the length value
            length = sizeof(buffer);
            break;
        default:
            apachews_stream_free(stream);
            return false;
        }
    }
    data = apachews_stream_data(stream);
    // Make a WebSocket frame
    frame = apachews_get_frame(data);
    // Forward frames to the server program,
    // except of course control frames, like
    // close or ping
    apachews_stream_free(stream);
    if (frame == NULL)
        return false;
    switch (frame->type) {
    case WSCloseFrame:
        // Close the client side socket
        apr_socket_shutdown(client, APR_SHUTDOWN_READWRITE);
        apr_socket_close(client);
        // Close the server side socket
        apr_socket_shutdown(server, APR_SHUTDOWN_READWRITE);
        apr_socket_close(server);
        free(frame);
        return false;
    case WSTextFrame:
        length = frame->length;
        status = apr_socket_send(server, (char *) frame->data, &length);
        if (status != APR_SUCCESS)
            err("could not send the data to the server program");
        free(frame->data);
        free(frame);
        return true;
    case WSInvalidFrame:
    case WSContinuationFrame:
    case WSBinaryFrame:
        return false;
    case WSPingFrame:
        err("ping frame received");
        return true;
    case WSPongFrame:
        err("pong frame received");
        return true;
    }
    return false;
}

static bool
apachews_read_from_server(apr_pool_t *pool, apr_socket_t *server, apr_socket_t *client)
{
    apachews_stream *stream;
    apr_status_t status;
    bool result;
    size_t length;
    char buffer[0x4000];
    apr_pollset_t *pset;
    apr_pollfd_t pollfd[] = {POLLFD_INITIALIZER};
    const char *string;
    bool reading;
    int count;
    // Create a poll set to know if we still need to read from
    // the socket
    if (apr_pollset_create(&pset, 1, pool, APR_POLLSET_DEFAULT) != APR_SUCCESS)
        return false;
    pollfd[0].desc.s = server;
    // Add the file descriptor to the set
    apr_pollset_add(pset, &pollfd[0]);
    // Setup a buffer to read the data
    stream = apachews_stream_new();
    if (stream == NULL)
        return false;
    // Set the initial value of the flag
    reading = true;
    // Set the initial value of the length
    length = sizeof(buffer);
    // This value is one, because there is only one socket
    // to read from
    count = 1;
    while (reading == true) {
        reading = (apr_pollset_poll(pset, count, &count, 0) == APR_SUCCESS);
        if (reading == false)
            continue;
        status = apr_socket_recv(server, buffer, &length);
        switch (status) {
        case APR_EAGAIN:
            break;
        case APR_SUCCESS:
            if (length == 0) {
                reading = false;
            } else if (apachews_stream_append(stream, buffer, length) == -1) {
                goto error;
            }
            // Reset the length value
            length = sizeof(buffer);
            break;
        default:
            goto error;
        }
    }
    if (apachews_stream_append(stream, "", 1) == -1)
        goto error;
    // If there is no data really, get out
    result = true;
    if (apachews_stream_length(stream) > 1) {
        // Forward the data to the websocket client
        string = (const char *) apachews_stream_data(stream);
        result = apachews_send_string(client, string, false);
        // Release temporary memory
        apachews_stream_free(stream);
    }
    return result;
error:
    apachews_stream_free(stream);
    return false;
}

static bool
apachews_check_socket(apr_pool_t *pool, const apr_pollfd_t *const ready,
          int32_t count, apr_socket_t *const server, apr_socket_t *const client)
{
    bool result;
    result = false;
    for (apr_int32_t i = 0 ; i < count ; ++i) {
        const apr_pollfd_t *next;
        // Make a pointer to the pollfd of interest
        next = &ready[i];
        // Here we simply check which one of the descriptors it is
        // this is very simple because there will always be only 2
        // descriptors
        //
        // Each one has a callback function associated that will
        // always perform the same action, simply push the data
        // forward to the interested party, the client or the
        // external service that is listening to the websocket
        // clients
        if (next->client_data == server) {
            result = apachews_read_from_server(pool, server, client);
        } else if (next->client_data == client) {
            result = apachews_read_from_client(pool, client, server);
        }
    }
    return result;
}

static bool
apachews_start(apr_socket_t *client, apr_pool_t *pool, const char *const path)
{
    apr_pollfd_t pollfd[] = {POLLFD_INITIALIZER, POLLFD_INITIALIZER};
    apr_pollset_t *pset;
    apr_socket_t *server;
    apr_int32_t count;
    apr_int32_t flags;
    bool alive;

    // Ensure that it will not block on read
    apr_socket_timeout_set(client, 0);
    // Loop control
    count = sizeof(pollfd) / sizeof(*pollfd);
    flags = APR_POLLSET_DEFAULT;
    // Create a poll set of two descriptors, this is all we need
    // to help the client communicate with the other process.
    if (apr_pollset_create(&pset, count, pool, flags) != APR_SUCCESS)
        return false;
    // Make the connection to the input server from where
    // the data will go to the websocket client.
    //
    // This should be bidirectional, but for now it will just
    // be unidirectional, data comming from the external program
    // will go to the client directly.
    //
    // The bidirectional idea is to do the same, in the other direction.
    //
    // This layer, will only deal with passing data between the
    // client and the data source or generation program.
    //
    // The bidirectionality will allow the clients to make requests.
    server = apachews_connect_to_data_server(pool, path);
    if (server == NULL)
        goto error;
    pollfd[0].desc.s = client;
    pollfd[0].client_data = client;

    apr_pollset_add(pset, &pollfd[0]);

    pollfd[1].desc.s = server;
    pollfd[1].client_data = server;

    apr_pollset_add(pset, &pollfd[1]);
    alive = true;
    while (alive == true) {
        const apr_pollfd_t *ready;
        apr_status_t status;
        status = apr_pollset_poll(pset, -1, &count, &ready);
        switch (status) {
        case APR_SUCCESS:
            alive = apachews_check_socket(pool, ready, count, server, client);
            break;
        default:
            alive = false;
            break;
        }
    }
    apr_pollset_destroy(pset);

    return true;
error:
    apr_pollset_destroy(pset);
    if (errno != 0)
        err("%s:%d: %s", __FUNCTION__, errno, strerror(errno));
    return false;
}

static int
apachews_handler(request_rec *request)
{
    apr_socket_t *sock;
    const apr_array_header_t *fields;
    apr_table_entry_t *entries;
    // Check of we should handle this
    if (strcmp(request->handler, "websocket") != 0)
        return DECLINED;
    // Get the request headers to negotiate with the
    // websocket client.
    fields = apr_table_elts(request->headers_in);
    entries = (apr_table_entry_t *) fields->elts;
    // Parse the headers and make a websocket negotiation
    for (int i = 0 ; i < fields->elt_size ; ++i) {
        char key[28];
        if ((entries[i].key == NULL) || (strcasecmp(entries[i].key, "null") == 0))
            continue;
        if (strcasecmp(entries[i].key, "sec-websocket-key") != 0)
            continue;
        // Make the accept key to notify the client
        // that we have accepted this connection and
        // are willing to convert it into a
        // websocket connection
        apachews_accept_key(entries[i].val, key);
        // Send other importante headers
        apr_table_merge(request->headers_out, "X-WebSocket-Server", "apachews");
        apr_table_merge(request->headers_out, "Sec-WebSocket-Accept", key);
        apr_table_merge(request->headers_out, "Upgrade", "websocket");
        apr_table_merge(request->headers_out, "Connection", "Upgrade");
        // Status should be "101 Switching Protocols"
        request->status = 101;
        request->status_line = "101 Switching Protocols";
        // Respond to the client now
        ap_send_interim_response(request, 1);
        ap_finalize_request_protocol(request);
        // Get a reference to the connection socket
        sock = ap_get_conn_socket(request->connection);
        if (sock != NULL) {
            // Call the main function to start forwarding
            // frames from/to the client/server
            //
            // Here, server refers to a program that will be
            // serving data to the websocket clients connected
            // to this socket
            apachews_start(sock, request->pool, request->filename);
        }
        return OK;
    }
    return HTTP_INTERNAL_SERVER_ERROR;
}

static void
apachews_register_hooks(apr_pool_t *pool)
{
    (void) pool;
    ap_hook_handler(apachews_handler, NULL, NULL, APR_HOOK_LAST);
}
