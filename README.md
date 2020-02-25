ZNC per-client channel module
=============================

This module attempts to isolate connected clients, so that each client connection looks like a separate connection to the target IRC server.

Currently, the extent of this isolation extends to the list of joined channels, so that each client only sees the list of (and activity from) the channels that it asked to join.

Effectively, this means the following:

- New connections to ZNC start with no joined channels.
  (Use your IRC client's auto-join feature to automatically join channels.)

- Joining a channel causes ZNC to join it (unless it is already joined, due to a different client having already joined it).

- Parting a channel causes ZNC to part it only if it is the last client which had the channel joined.

- Channel messages are sent only to clients which have the channel joined.

If you would like to have ZNC automatically join and stay in a channel regardless of whether any clients have it open, add it to the ZNC configuration as a saved + detached + enabled channel.

This module is incompatible with the [chansaver](https://wiki.znc.in/Chansaver) module for obvious reasons.

This module is originally based on the [chanfilter](https://wiki.znc.in/Chanfilter) module by jpnurmi@gmail.com.

Limitations
-----------

- This module is insufficient for full isolation, as some replies (e.g. LIST) are still sent to all clients.
  The [route_replies](https://wiki.znc.in/Route_replies) module may help with this.

- QUIT replies are not filtered; i.e. QUIT messages are always visible to all clients, regardless whether the client is in any channels where the quitting user was present.

- Channels must be joined with the same case as they exist on the server.
