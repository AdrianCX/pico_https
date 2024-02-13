#include <string.h>
#include <time.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "request_handler.h"
#include "wifi.h"

#include "mbedtls_wrapper.h"
#include "pico_logger.h"

#include "rc/index.html.hex.h"

const unsigned int HTTP_BODY_LEN = _source_hello_world_rc_index_html_len;
const u8_t *HTTP_BODY = &_source_hello_world_rc_index_html[0];

Session *RequestHandler::create(void *arg) {
    return new RequestHandler(arg);
}

RequestHandler::RequestHandler(void *arg)
    : HTTPSession(arg)
{
    trace("RequestHandler::RequestHandler: this=%p, arg=%p\n", this, arg);
}

bool RequestHandler::onRequestReceived(HTTPHeader& header)
{
    if (strcmp(header.getCommand(), "GET") != 0)
    {
        trace("RequestHandler::onRequestReceived: this=%p, unexpected command[%s]:\n", this, header.getCommand());
        header.print();
        return false;
    }

    if (strcmp(header.getPath(), "/websocket") == 0)
    {
        trace("RequestHandler::onRequestReceived: this=%p, acceptWebSocket.\n", this);
        return acceptWebSocket(header);
    }
    else
    {
        trace("RequestHandler::onRequestReceived: this=%p, sendReply[index.html]:\n", this);
        return sendHttpReply((const char *)HTTP_BODY, HTTP_BODY_LEN);
    }
}

bool RequestHandler::onHttpData(u8_t *data, size_t len)
{
    trace("RequestHandler::onHttpData: this=%p, data[%s] len[%d]:\n", this, data, len);
    return true;
}

bool RequestHandler::onWebSocketData(u8_t *data, size_t len)
{
    trace("RequestHandler::onWebSocketData: this=%p, data[%.*s]:\n", this, len, data);
    return true;
}
