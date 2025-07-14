#include <libwebsockets.h>
#include <string.h>

static int callback(struct lws *wsi, enum lws_callback_reasons reason,
                    void *user, void *in, size_t len)
{
    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED:
            lwsl_user("Cliente conectado\n");
            break;

        case LWS_CALLBACK_SERVER_WRITEABLE: {
            const char *msg = "Hola desde Luckfox\n";
            unsigned char buf[LWS_PRE + 128] = {0};
            size_t msg_len = strlen(msg);

            memcpy(&buf[LWS_PRE], msg, msg_len);
            lws_write(wsi, &buf[LWS_PRE], msg_len, LWS_WRITE_TEXT);
            break;
        }

        default:
            break;
    }
    return 0;
}

static struct lws_protocols protocols[] = {
    {
        .name = "my-protocol",
        .callback = callback,
        .per_session_data_size = 0,
        .rx_buffer_size = 0,
    },
    { NULL, NULL, 0, 0 } // terminador
};

int main(void) {
    struct lws_context_creation_info info = {0};
    info.port = 9000;
    info.protocols = protocols;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

    struct lws_context *context = lws_create_context(&info);
    if (!context) {
        lwsl_err("Error creando el contexto\n");
        return 1;
    }

    lwsl_user("Servidor WebSocket escuchando en ws://localhost:9000\n");

    while (1)
        lws_service(context, 0);

    lws_context_destroy(context);
    return 0;
}
