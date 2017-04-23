#ifndef __PHP_APACHEWS_H__
#define __PHP_APACHEWS_H__

#include <php.h>

#if ZEND_MODULE_API_NO >= 20010901
#    define MODULE_VERSION "1.0.0",
#    define MODULE_HEADER STANDARD_MODULE_HEADER,
#else
#    define MODULE_VERSION
#    define MODULE_HEADER
#endif

#if PHP_MAJOR_VERSION == 7
#   define STRLEN(x) (sizeof((x)) - 1)
#   define WS_RETURN_STRING(x, y) RETURN_STRINGL((x), (y));
#   define zend_rsrc_list_entry zend_resource
#   define ZEND_REGISTER_RESOURCE(x, y, z) ZVAL_RES(x, zend_register_resource(y, z))
#   define APACHEWS_ZVAL(x) (*(x))
#   define apachews_FETCH_RESOURCE(x, y, z) zend_fetch_resource_ex(APACHEWS_ZVAL((x)),  (y), (z))
#   define PHP_APACHEWS_GET_OBJECT(type, obj) ((type *) ((char *) obj - XtOffsetOf(type, parent)))
#   define Z_PHP_APACHEWS_GET_OBJECT(type, obj) PHP_APACHEWS_GET_OBJECT(type, Z_OBJ_P(obj))
#   define PHP_OBJECT_HEADER
#   define PHP_OBJECT_FOOTER zend_object parent;
#   define SIZEOF_CE(ice) sizeof(ice) + zend_object_properties_size(ice ## _ce)
#    define PHP_APACHEWS_RETURN_STRINGL(s, l) RETURN_STRINGL(s, l)
#    define ZEND_OBJECT zend_object *
#    define PHP_APACHEWS_EMPTY_ZEND_OBJECT NULL
#    define PHP_APACHEWS_SET_HANDLERS(zov, name) (zov)->handlers = &(php_apachews_ ## name ## _handlers)
#    define PHP_APACHEWS_INITIALIZE_OBJECT(zov, object) zov = &object->parent
#    define PHP7_INITIALIZER(name)                           \
        name ## _handlers.offset = XtOffsetOf(name, parent); \
        name ## _handlers.dtor_obj = name ## _dtor
#else
static zend_object_value php_apachews_empty_zend_object_value;
#   define PHP_OBJECT_HEADER zend_object parent;
#   define PHP_OBJECT_FOOTER
#   define STRLEN(x) (sizeof((x)))
#   define WS_RETURN_STRING(x, y) RETURN_STRINGL((x), (y), 1);
#   define APACHEWS_ZVAL(x) (x)
#   define Z_PHP_APACHEWS_GET_OBJECT(type, obj) zend_object_store_get_object(obj TSRMLS_CC)
#   define PHP_APACHEWS_GET_OBJECT(type, obj) (type *) obj
#   define apachews_FETCH_RESOURCE(x, y, z) \
        zend_fetch_resource(APACHEWS_ZVAL((x)) TSRMLS_CC, -1,  (y), NULL, 1, (z))
#   define SIZEOF_CE(ice) sizeof(ice)
#    define PHP7_INITIALIZER(name)
#    define PHP_APACHEWS_RETURN_STRINGL(s, l) RETURN_STRINGL(s, (int) l, 1)
#    define ZEND_OBJECT zend_object_value
#    define PHP_APACHEWS_EMPTY_ZEND_OBJECT php_apachews_empty_zend_object_value
#    define PHP_APACHEWS_SET_HANDLERS(zov, name) (zov).handlers = &(php_apachews_ ## name ## _handlers)
#    define PHP_APACHEWS_INITIALIZE_OBJECT(zov, object) (zov).handle = zend_objects_store_put(object, (zend_objects_store_dtor_t) zend_objects_destroy_object, (zend_objects_free_object_storage_t) zend_objects_free_object_storage, NULL TSRMLS_CC)
#endif

#define CREATE_CLASS_ENTRY(ce, name, type) do {                                                     \
        INIT_CLASS_ENTRY(ce, "apachews\\" name, type ## _methods);                                  \
        type ## _ce = zend_register_internal_class(&ce TSRMLS_CC);                                  \
        type ## _ce->create_object = type ## _new;                                                  \
        memcpy(&(type ## _handlers), zend_get_std_object_handlers(), sizeof(zend_object_handlers)); \
        PHP7_INITIALIZER(type);                                                                     \
    } while (0)

#define argc ZEND_NUM_ARGS() TSRMLS_CC
#define __CONST_DEF(x) #x, STRLEN(#x), x
#define apachews_register_enum(_const_, flags, modnum) zend_register_long_constant(__CONST_DEF(_const_), flags, modnum TSRMLS_CC);

#define DefinePHPObject(name, type)        \
    typedef struct php_apachews_ ## name { \
        PHP_OBJECT_HEADER                  \
        apachews_ ## type *type;           \
        PHP_OBJECT_FOOTER                  \
    } php_apachews_ ## name
#endif // __PHP_APACHEWS_H__
