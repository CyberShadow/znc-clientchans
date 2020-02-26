## Notes

- It's easier to see what's going on with debug logging.
  Change the `#if 0` to `#if 1` and `tail -F /tmp/clientchans.log`.

## Test plan

- [ ] Basic operations
  - [ ] Joining and parting channels (with a single client) should work as expected.
  - [ ] Parting channels should propagate the PART to the server.
- [ ] Persistent channels
  - [ ] (Prerequisite: saved + enabled + detached channel in the ZNC config)
  - [ ] ZNC should join the channel by itself even with no clients connected
  - [ ] First client to join the channel should work, and shouldn't generate a JOIN to the server
  - [ ] Parting the channel should work, shouldn't propagate the PART to the server
- [ ] Multiple clients
  - [ ] Joining a channel that's already open in another client shouldn't generate another JOIN to the server
  - [ ] With a channel open in multiple clients, parting it in one should leave it open in the other
- [ ] Quitting
  - [ ] Quitting / killing a client should cause ZNC to PART all non-persistent channels that were open only in that client
- [ ] Server joins/parts
  - [ ] Adding a channel in ZNC's web interface should have no effect on the clients (but still make ZNC join them as detached)
  - [ ] Removing a channel in ZNC's web interface should send a PART to those clients which have it open

