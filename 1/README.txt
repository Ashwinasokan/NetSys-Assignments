Client & Server examples given are improved to handle data loss by simple stop & wait protocol.

There are two improvements done to previous version,
1. There was a typo bug in previous version. line no 44 in server I used "STOP" as delimiter for the message in server but used "WAIT" in client.
2. I fixed the makefile bugs to make sure its clean.
