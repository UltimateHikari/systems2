## Caching HTTP Proxy

* uses [Pico](https://github.com/h2o/picohttpparser) for headers parsing
* uses [c-logger](https://github.com/yksz/c-logger) for MT-safe stderr logs
* works as HTTP 1.0
* has strange GC on cache
* cache based on linked list for simplicity sake
