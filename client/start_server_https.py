#!/usr/bin/env python3

import http.server
import ssl

PORT = 4443

handler = http.server.SimpleHTTPRequestHandler
httpd = http.server.HTTPServer(('0.0.0.0', PORT), handler)

context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
context.load_cert_chain(certfile="certs/cert.pem", keyfile="certs/key.pem")
httpd.socket = context.wrap_socket(httpd.socket, server_side=True)

print(f"Serving on https://0.0.0.0:{PORT}")
httpd.serve_forever()
