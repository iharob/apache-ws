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
