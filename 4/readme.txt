pages_from_server - PASS
pages_from_proxy  - PASS
pages_from_server_after_cache_timeout - PASS
prefetching       - PASS
multithreading  - PASS
connection keepalive    -  PASS

Attached file (helper.txt) lists the commands that need to be executed to run the above test cases.

Video Recording execution of test cases: https://www.youtube.com/watch?v=PVWO2196p7s

Multi clients handling: Mutiple client are handled in parallel by forking the request as soon as we got one meanwhile leaving the parent process to keep listening for future request. This allows the parent to continiously listen for requests infinitely. Each request gets mapped to separate process to get served.

Cache: Caching is implemented by saving the web file when retrieved from the remote server locally with md5 hashname of respective path. Everytime when a request check whether a file exist with creation time recent than the time out configured. If there is a file the file is read from local server and returned to the client. If there is no file or if the file is not recent than configured timeout original file from the server is again fetched. Its been decided to store the file in the local server rather than keep in memory to avoid malicious clients from loading the server program's memory. Its always a good idea not to let program's user control the memory footprint of any program. Also its stored as binary file with name = md5sum(path). This makes sure we are not picking any wrong entry from the cache.

PreFetch: Prefetching is implemented by forking a separate process which will read the contents of the requested web page, scan all valid links present in the page and prefetch all the links in advance anticipating request from the user. Since this activity is implemented with in a forked region in the code, this activity happens in background not obstructing the parent thread which is serving the actual user's request.

Connection KeepAlive: KeepAlive is implemented by reading the protocol whether its 1.1 or 1.0. If its 1.1 connection is not closed by kept open for next request. On the other hand if the protocol is 1.0, the connection is closed after serving once.

