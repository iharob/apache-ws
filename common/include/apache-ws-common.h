#ifndef __apachews_COMMON_H__
#define __apachews_COMMON_H__

#if defined (_WIN32)
#    include <WinSock2.h>
#    define MSG_DONTWAIT 0
#    define MSG_NOSIGNAL 0
#    define EXPORT __declspec(dllexport)
    typedef SSIZE_T ssize_t;
#else
#    include <sys/socket.h>
#   include <stdint.h>
#    include <stdbool.h>
    typedef int SOCKET;
    typedef int32_t DWORD;
    typedef bool BOOL;
#    define EXPORT
#    define INVALID_SOCKET (-1)
#    define SOCKET_ERROR (-1)
#    define closesocket(x) do {shutdown((x), SHUT_RDWR); close((x));} while(0)
#endif

#define SOCKET_DISCONNECTED 0
typedef struct apachews_stream apachews_stream;
typedef enum apachews_event_type {
    ApacheWSInvalidEvent = 8000,
    ApacheWSAcceptEvent, 
    ApacheWSIOEvent
} apachews_event_type;

typedef enum apachews_status {
    ApacheWSSuccess,
    ApacheWSNoData = 1001,
    ApacheWSConnectionClosed,
    ApacheWSOutOfMemory,
    ApacheWSError,
    ApacheWSWouldBlock,
    ApacheWSOther
} apachews_status;

typedef struct apachews_event apachews_event;
typedef struct apachews_context apachews_context;
typedef struct apachews_client apachews_client;
// Context
apachews_context *apachews_create(const char *const path);
void apachews_context_free(apachews_context *ctx);
void apachews_context_remove_client(apachews_context *ctx, const apachews_client *const  client);
apachews_event *apachews_server_next_event(apachews_context *ctx);
ssize_t apachews_server_broadcast(const apachews_context *const ctx, const uint8_t *const data, size_t length);
const char *apachews_client_get_language(const apachews_client * const ctx);

// Event
const apachews_context *apachews_event_get_context(const apachews_event *const event);
apachews_client *apachews_event_get_client(const apachews_event *const event);
apachews_event_type apachews_event_get_type(const apachews_event *const event);
void apachews_event_free(apachews_event *event);
apachews_status apachews_event_read(const apachews_event *const event, uint8_t **data, size_t *length);
ssize_t apachews_event_respond(const apachews_event *const event, uint8_t *data, size_t length);

// Client
ssize_t apachews_client_send(const apachews_client *const client, const void *const data, size_t length);
int apachews_client_close(apachews_client *const client);
SOCKET apachews_client_get_socket(const apachews_client *const client);
void apachews_client_free(apachews_client *const client);

// Stream
apachews_stream *apachews_stream_new(void);
int apachews_stream_resize(apachews_stream *stream, size_t hint);
void apachews_stream_free(apachews_stream *stream);
int apachews_stream_append(apachews_stream *stream, const char *const what, size_t length);
uint8_t *apachews_stream_data(const apachews_stream *const stream);
size_t apachews_stream_length(const apachews_stream *const stream);

// OS
void apachews_initialize_os(void);
#endif // __apachews_COMMON_H__
