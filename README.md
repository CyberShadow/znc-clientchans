A channel filter module for ZNC
===============================

### NOTICE

**This module is WIP!** It requires unmerged changes to ZNC:
[#687](https://github.com/znc/znc/pull/687). Consider the
current implementation as a proof of concept. The author is
well aware that the implementation is far from optimal, but
no further effort is put on this until the necessary change
will land to ZNC.

### Overview

The channel filter module maintains client specific channel lists
for identified clients. A typical use case is to have a subset of
channels for a mobile client.

### Usage

The module detects identified clients automatically, and starts
maintaining client specific lists of channels. It is possible to
manage the list of identified clients using the following module
commands:

    /msg *chanfilter addclient <identifier>
    /msg *chanfilter removeclient <identifier>
    /msg *chanfilter listclients

When an identified client connects ZNC first time, no channels are
automatically joined. In other words, all channels are filtered out.
The list of channels is updated when the identified client joins and
parts channels. Next time the identified client connects, it will
automatically join the channels it had active from the last session.

### Identifiers

ZNC supports passing a client identifier in the password:

    username@identifier/network:password

or in the username:

    username@identifier/network

### Contact

Got questions? Contact jpnurmi@gmail.com or *jpnurmi* on Freenode.
