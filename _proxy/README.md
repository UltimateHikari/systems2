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

### TODOs:
* restart of spontaniously dead server_connection by timeout from `client_connection::wait_on_lock_cond`
    * not required by task and is hard to test
* handle requests without content-length better
    * switch to proxy mode for all incoming clients, meet huge lag on re-proxying for reading cache_entry until BE_INF
    * not *actually* required by task
* better `Cache::eligible_to_collect` check, doesnt check server_connection state
    * nothing huge, prob just causes random NULL error in server, when could stop gracefully
* potentially make `server_connection::lag_broadcast` not void, for error checks
