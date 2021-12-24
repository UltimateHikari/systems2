## Caching HTTP Proxy

* uses [Pico](https://github.com/h2o/picohttpparser) for headers parsing
* uses [c-logger](https://github.com/yksz/c-logger) for MT-safe stderr logs
* works as HTTP 1.0
* has strange GC on cache
* cache based on linked list for simplicity sake
* parallel download on client drop hadled trivially, by shutting only client connection down on client drop
    <details>
    <summary>Original task</summary>

    Прокси должен корректно обрабатывать сброс клиентских сессий. В том числе, в случае, когда две или более сессий работали с одной записью кэша, после сброса одной из них, остальные сессии должны корректно продолжить докачку страницы.

