# apache-ws
WebSocket server implemented as an Apache2 module

This is a apache module that implements the websocket protocol, the only purpose of the module is to convert a regular HTTP connection into a WebSocket connection.

The module interacts with any program that is able to create a Unix Domain Socket (*on Linux or any OS with support for Unix Domain Sockets*) or an emulation of UDS on Windows OS.

To simplify this process, a library is shipped that allows transparent creation of this IPC channel between the Apache server and our own WebSocket "*producer*" program. And it would also permit our implementation to perform IO operations as if it were performing them directly on the WebSocket connection, although technically we would be doing this through a proxy WebSocket connection that would be the Apache Web Server.

The motivation to implement this like I did, is that apache handles HTTP for us so the handshake process is rather simple and also because Apache takes care of the TCP connection for us, so this was simple to implement and it is simple to use. Not to mention, that the underlying software that handles the most important part is fairly reliable and robust.

To activate the module you just need to use the [LoadModule](https://httpd.apache.org/docs/2.4/mod/mod_so.html) Apache2 configuration directive and add a handler to your [apache directory configuration](http://httpd.apache.org/docs/current/mod/core.html#directory), for example

    <Directory /var/www/public_html>
        AddHandler websocket .ws
        # Rest of settings
    </Directory>

# PHP Extension

A "*toy*" implementations of a PHP extension is also contained in this package, it allows writting a PHP WebSocket capable program with very few lines of code, the simplest WebSocket program is one that "*echoes*" what the client sends back to the client, such a program is implemented with our PHP extension with this simple code

	<?php
	$path = "/home/iharob/www/hti/hti.ws";
	if (file_exists($path))
		unlink($path);
	$ws = new apachews\Server($path);
	while ($event = $ws->dequeue()) {
		switch ($event->type()) {
		case ApacheWSAcceptEvent:
			// Ignore this, or put the connection in an array
			// to close it when we want, or write to it when
			// we need.
			break;
		case ApacheWSIOEvent:
			$data = $event->read();
			// The read function could return an error, for
			// instance if the client suddenly disconnects
			switch ($data) {
				case ApacheWSError:
				case ApacheWSConnectionClosed:
					break;
				default:
					$event->write($data);
					break;
			}
			break;
		case ApacheWSNoData:
			// Nothing received, so perhaps we can do something
			// special in this case
			break;
		case ApacheWSError:
			// An error occurred
			break;
		case ApacheWSConnectionClosed:
			break;
		}
	}
	?>

    
This given that your site is at `/var/www/public_html`, you can then connect to it from javascript like this
    
    var webSocket = new WebSocket('ws://' + window.location.host + '/websocket.ws');
    
you can see then, how simple it is to use this.

# Python Module

There is as well, a python module that allows writing *producer* programs with python. It is just as simple to write the echo program,

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

# Final Notes

Note that these are really *toy* implementations to illustrate how to use the module, the code of course can be used as the base for a full featured PHP extension or Python module or as the basis for a different kind of binding.

It's also noteworthy that it can be used as an example on how to write a c program that uses this module and it's interface, I actually used it for a c program and it was a great success, unfortunately I am not allowed to put that program in a repository here.
