This is kinda interesting so let's plan it out here

Per request:
Let's have a main process which parses args for worker config
 - num_workers (default 4)
 - stay_alive time (default 60 s)

Workers pool requests into buffer until complete. 
Workers then parse HTTP request (headers)
Then workers call framework function with this info on separate thread
