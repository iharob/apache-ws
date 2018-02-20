#include <apache-ws-common.h>
#include <php-apachews.h>

#define BUFFER_SIZE 0x4000

#define ApacheWSSocketST "ApacheWSSocket"
#define ApacheWSContextST "ApacheWSContext"
#define ApacheWSEventST "ApacheWSEvent"
#define ApacheWSClientST "ApacheWSClient"

static ZEND_OBJECT php_apachews_server_new(zend_class_entry *ce TSRMLS_DC);
static ZEND_OBJECT php_apachews_event_new(zend_class_entry *ce TSRMLS_DC);
static ZEND_OBJECT php_apachews_client_new(zend_class_entry *ce TSRMLS_DC);

static void php_apachews_server_dtor(zend_object *object);
static void php_apachews_event_dtor(zend_object *object);
static void php_apachews_client_dtor(zend_object *object);

DefinePHPObject(server, context);
DefinePHPObject(event, event);
DefinePHPObject(client, client);

PHP_METHOD(Server, __construct);
PHP_METHOD(Server, dequeue);
PHP_METHOD(Server, broadcast);
PHP_METHOD(Server, language);
// Define `server' object methods
static const zend_function_entry php_apachews_server_methods[] = {
    PHP_ME(Server, __construct, NULL, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    PHP_ME(Server, dequeue, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Server, broadcast, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Server, language, NULL, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

PHP_METHOD(Event, getClient);
PHP_METHOD(Event, read);
PHP_METHOD(Event, respond);
PHP_METHOD(Event, type);
// Define `Event' object methods
static const zend_function_entry php_apachews_event_methods[] = {
    PHP_ME(Event, getClient, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Event, read, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Event, respond, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Event, type, NULL, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

PHP_METHOD(Client, send);
PHP_METHOD(Client, close);
// Define `Client' object methods
static const zend_function_entry php_apachews_client_methods[] = {
    PHP_ME(Client, send, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Client, close, NULL, ZEND_ACC_PUBLIC)
    PHP_FE_END
};
static zend_class_entry *php_apachews_server_ce;
static zend_object_handlers php_apachews_server_handlers;
static zend_class_entry *php_apachews_event_ce;
static zend_object_handlers php_apachews_event_handlers;
static zend_class_entry *php_apachews_client_ce;
static zend_object_handlers php_apachews_client_handlers;

// Disable debugging
#define ZEND_DEBUG 0

PHP_MINIT_FUNCTION(apachews_php);
zend_module_entry php_apachews_module_entry = {
    MODULE_HEADER "apachews",
    NULL,
    PHP_MINIT(apachews_php),
    NULL,
    NULL,
    NULL,
    NULL,
    MODULE_VERSION STANDARD_MODULE_PROPERTIES
};

ZEND_GET_MODULE(php_apachews)
PHP_MINIT_FUNCTION(apachews_php)
{
    int modnum;
    zend_class_entry ce;
    int flags;
    // Create 'Server' Class Entry
    CREATE_CLASS_ENTRY(ce, "Server", php_apachews_server);
    // Create 'Event' Class Entry
    CREATE_CLASS_ENTRY(ce, "Event", php_apachews_event);
    // Create 'Client' Class Entry
    CREATE_CLASS_ENTRY(ce, "Event", php_apachews_client);
    // OS specific initialization
    apachews_initialize_os();
    // Remove the ports file if it exists
    // otherwise fail silently
    flags = CONST_PERSISTENT | CONST_CS;
    // Store the module number (to avoid repeating)
    modnum = php_apachews_module_entry.module_number;
    // Create PHP constants
    apachews_register_enum(ApacheWSError, flags, modnum);
    apachews_register_enum(ApacheWSNoData, flags, modnum);
    apachews_register_enum(ApacheWSConnectionClosed, flags, modnum);
    apachews_register_enum(ApacheWSAcceptEvent, flags, modnum);
    apachews_register_enum(ApacheWSIOEvent, flags, modnum);

    return SUCCESS;
}

ZEND_OBJECT
php_apachews_server_new(zend_class_entry *ce TSRMLS_DC)
{
    php_apachews_server *object;
    ZEND_OBJECT result;
    object = ecalloc(1, SIZEOF_CE(php_apachews_server));
    result = PHP_APACHEWS_EMPTY_ZEND_OBJECT;
    if (object == NULL)
        return result;
    zend_object_std_init(&object->parent, ce TSRMLS_CC);
    object_properties_init(&object->parent, ce);

    PHP_APACHEWS_INITIALIZE_OBJECT(result, object);
    PHP_APACHEWS_SET_HANDLERS(result, server);

    return result;

}

static void
php_apachews_server_dtor(zend_object *object)
{
    php_apachews_server *instance;
    instance = PHP_APACHEWS_GET_OBJECT(php_apachews_server, object);
    // Free the context
    apachews_context_free(instance->context);
    // Unset every thing
    memset(instance, 0, SIZEOF_CE(php_apachews_server));
    // Now, release memory
    efree(instance);
}

ZEND_OBJECT
php_apachews_event_new(zend_class_entry *ce TSRMLS_DC)
{
    php_apachews_event *object;
    ZEND_OBJECT result;
    object = ecalloc(1, SIZEOF_CE(php_apachews_event));
    result = PHP_APACHEWS_EMPTY_ZEND_OBJECT;
    if (object == NULL)
        return result;
    zend_object_std_init(&object->parent, ce TSRMLS_CC);
    object_properties_init(&object->parent, ce);

    PHP_APACHEWS_INITIALIZE_OBJECT(result, object);
    PHP_APACHEWS_SET_HANDLERS(result, event);

    return result;
}

static void
php_apachews_event_dtor(zend_object *object)
{
    php_apachews_event *instance;
    instance = PHP_APACHEWS_GET_OBJECT(php_apachews_event, object);
    fprintf(stderr, "destroying event instance\n");
    // Free the event
    apachews_event_free(instance->event);
    // Unset every thing
    memset(instance, 0, SIZEOF_CE(php_apachews_event));
    // Now, release memory
    efree(instance);
}

ZEND_OBJECT
php_apachews_client_new(zend_class_entry *ce TSRMLS_DC)
{
    php_apachews_event *object;
    ZEND_OBJECT result;
    object = ecalloc(1, SIZEOF_CE(php_apachews_event));
    result = PHP_APACHEWS_EMPTY_ZEND_OBJECT;
    if (object == NULL)
        return result;
    zend_object_std_init(&object->parent, ce TSRMLS_CC);
    object_properties_init(&object->parent, ce);

    PHP_APACHEWS_INITIALIZE_OBJECT(result, object);
    PHP_APACHEWS_SET_HANDLERS(result, event);

    return result;
}

static void
php_apachews_client_dtor(zend_object *object)
{
    php_apachews_client *instance;
    instance = PHP_APACHEWS_GET_OBJECT(php_apachews_client, object);
    fprintf(stderr, "destroying client instance\n");
    // Free the event
    apachews_client_free(instance->client);
    // Unset every thing
    memset(instance, 0, SIZEOF_CE(php_apachews_event));
    // Now, release memory
    efree(instance);
}

PHP_METHOD(Server, __construct)
{
    int result;
    zval self;
    php_apachews_server *server;
    zval *path;
    // Extract the parameters, the path is mandatory
    // so FAILURE will be returned in case it's not
    // passed
    result = zend_parse_method_parameters(argc, getThis(), "Oz",
                                          &self, php_apachews_server_ce, &path);
    // Make a pointer to the "actual object"
    server = Z_PHP_APACHEWS_GET_OBJECT(php_apachews_server, getThis());
    if (result == FAILURE)
        RETURN_FALSE;
    // Initialize this
    server->context = apachews_create(Z_STRVAL_P(path));
    if (server->context == NULL)
        zend_error(E_ERROR, "cannot create the server %s");
}

PHP_METHOD(Server, language)
{
}

PHP_METHOD(Server, dequeue)
{
    php_apachews_server *instance;
    php_apachews_event *result;
    // Instance can't be `NULL' or can it?
    instance = Z_PHP_APACHEWS_GET_OBJECT(php_apachews_server, getThis());
    // Initialize the event object.
    //
    // The policy is, to always return an
    // event. In case of error the event
    // member will be `NULL' such that Event::type()
    // will return `ApacheWSInvalidEvent'
    object_init_ex(return_value, php_apachews_event_ce);
    result = Z_PHP_APACHEWS_GET_OBJECT(php_apachews_event, return_value);
    result->event = apachews_server_next_event(instance->context);
}

PHP_METHOD(Server, broadcast)
{
    zval *string;
    zval *self;
    php_apachews_server *instance;
    int result;
    // Instance can't be `NULL' or can it?
    instance = Z_PHP_APACHEWS_GET_OBJECT(php_apachews_server, getThis());
    result = zend_parse_method_parameters(argc, getThis(), "Oz",
                                         &self, php_apachews_event_ce, &string);
    if (result == FAILURE)
        return;
    if (apachews_server_broadcast(instance->context,
             (const uint8_t *) Z_STRVAL_P(string), Z_STRLEN_P(string)) == -1) {
        RETURN_FALSE;
    }
    RETURN_TRUE;
}

// This should be `Client' class
PHP_METHOD(Event, read)
{    
    php_apachews_event *instance;
    size_t length;
    uint8_t *data;
    instance = Z_PHP_APACHEWS_GET_OBJECT(php_apachews_event, getThis());
    if (instance->event == NULL)
        RETURN_LONG(ApacheWSError);
    switch (apachews_event_read(instance->event, &data, &length)) {
    case ApacheWSConnectionClosed:
        RETURN_LONG(ApacheWSConnectionClosed);
    case ApacheWSSuccess:
        PHP_APACHEWS_RETURN_STRINGL((char *) data, length);
        break;
    default:
        RETURN_LONG(ApacheWSError);
    }
}

// This should be `Client' class
PHP_METHOD(Event, respond)
{
    size_t length;
    uint8_t *data;
    php_apachews_event *instance;
    int result;
    zval *string;
    zval *self;
    result = zend_parse_method_parameters(argc, getThis(), "Oz",
                                         &self, php_apachews_event_ce, &string);
    if (result == FAILURE)
        return;
    instance = Z_PHP_APACHEWS_GET_OBJECT(php_apachews_event, getThis());
    data = (uint8_t *) Z_STRVAL_P(string);
    length = Z_STRLEN_P(string);
    if (instance->event == NULL)
        RETURN_LONG(ApacheWSInvalidEvent);
    if (apachews_event_respond(instance->event, data, length) == ApacheWSSuccess)
        RETURN_TRUE;
    RETURN_FALSE;
}

PHP_METHOD(Event, type)
{
    php_apachews_event *instance;
    instance = Z_PHP_APACHEWS_GET_OBJECT(php_apachews_event, getThis());
    if (instance->event == NULL)
        RETURN_LONG(ApacheWSInvalidEvent);
    RETURN_LONG(apachews_event_get_type(instance->event));
}

PHP_METHOD(Event, getClient)
{
    php_apachews_client *result;
    // const apachews_context *ctx;
    php_apachews_event *self;
    // const apachews_client *client;
    self = Z_PHP_APACHEWS_GET_OBJECT(php_apachews_event, getThis());
    if (self->event == NULL)
        RETURN_FALSE;
    object_init_ex(return_value, php_apachews_client_ce);
    result = Z_PHP_APACHEWS_GET_OBJECT(php_apachews_client, return_value);
    result->client = apachews_event_get_client(self->event);
}

// This should be `Client' class
PHP_METHOD(Client, send)
{
    size_t length;
    uint8_t *data;
    php_apachews_client *self;
    int result;
    zval *string;
    // zval *self;
    result = zend_parse_method_parameters(argc, getThis(), "Oz",
                                         &self, php_apachews_event_ce, &string);
    if (result == FAILURE)
        return;
    self = Z_PHP_APACHEWS_GET_OBJECT(php_apachews_client, getThis());
    data = (uint8_t *) Z_STRVAL_P(string);
    length = Z_STRLEN_P(string);
    if (self->client == NULL)
        RETURN_FALSE;
    if (apachews_client_send(self->client, data, length) == length)
        RETURN_LONG(length);
    RETURN_FALSE;
}

PHP_METHOD(Client, close)
{
    php_apachews_client *self;
    self = Z_PHP_APACHEWS_GET_OBJECT(php_apachews_client, getThis());
    if (self->client == NULL)
        RETURN_FALSE;
    apachews_client_close(self->client);
}
