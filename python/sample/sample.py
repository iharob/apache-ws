from apachews import *
import os

path = '/var/www/public_html/websocket.ws'
server = Server(path)
while True:
    event = server.dequeue()
    if event.type() == Event.Accept:
        # This just means, that a new connection
        # was accepted. We could put this object
        # in an array to interact with this client
        # at will
        pass
    elif event.type() == Event.ConnectionClosed:
        # The connection was closed, close it from
        # this side too
        event.close()
    elif event.type() == Event.IO:
        data = event.read()
        # The read function could return an error,
        # for example if the clients suddenly disconnects
        if data == Event.ConnectionClosed:
            event.close()
        elif data == Event.Error:
            pass
        else:
            event.write(data)
    elif event.type() == Event.Error:
        # An error occurred
        pass
    elif event.type() == Event.NoData:
        # An empty frame was received
        pass
