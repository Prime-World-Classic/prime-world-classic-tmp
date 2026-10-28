# Быстрый реконнект к любому серверу пула (без вылета из боя)

Ветка `fast-reconnect` (от `main`). Цель: при потере соединения клиент за
несколько секунд переподключается к **тому же** бою, используя любой доступный
IP пула — целевой сервер остаётся один, остальные проксируют трафик.
Допустимо кратковременное зависание картинки, недопустим вылет в лобби/ошибку.

## Что уже есть (не переделывать)

| Механизм | Где |
|---|---|
| Проксирование портов пула (DNAT+MASQUERADE по `game_servers.json`, тикер 60 с) | `PW_GAME_SERVER_DOCKER/entrypoint.sh` (`PW_REDIRECT_IN/OUT/FWD`) |
| XDP-гейт пропускает весь диапазон портов пула (27300–27999) | `PW_GAME_SERVER_DOCKER/xdp/xdp_udp_guard.c` |
| Бэкенд выдаёт клиенту весь alive-пул (hex-блок) + base-порт целевого | `pw-api/objects/sessionLaunch.js`, `gameServers.js:poolBlock()` |
| Клиент строит адреса из блока: `GetServerIpA(usedServer)`, login=base+1@10, front=base+10 | `Src/Network/ClusterConfiguration.cpp:52-90`, `Src/Shared/ServerIps.cpp` |
| Обход пула при первичном логине (`usedServer++` при отказе) | `Src/PW_Client/GameContext.cpp:344-356` |
| Сервер держит бой отвалившегося клиента: резерв снапшотов, `AddClientFast`, `OnRejoinClient`, clientctrl «kick previous» | `Profiles/pvx_srv_gamesvc.cfg` (`game_fast_reconnect_reserve_time 50.0`, `game_reconnect_enabled 1`, `game_reconnect_timeout 60.0`), `Src/Game/PF/HybridServer/{ClientHolder,PeeredImpl}.cpp`, `Src/Server/ClientControl/ClientControlLogic.cpp:184-223` |
| Клиентская машина fast reconnect (ctx с картой/`transceiver`/`ServerNode`/replay/статистикой/чатом, возобновление с `fromStep`, счётчик попыток в UI) | `Src/PW_Client/FastReconnectCtxPW.h`, `Src/Game/PF/Client/LobbyPvx/{FastReconnectCtx,LobbyGameClient}.cpp` |

## Найденные дыры (почему сейчас вылет)

1. **Fast reconnect выключен навсегда**: `GameContext::ShouldStartFastReconnect()`
   требует `socialMode`, а `socialMode` в конструкторе зафиксирован в `false`
   (`Src/PW_Client/GameContext.cpp:90`, `ParseSessionKey` закомментирован).
2. **После реконнекта бой не подхватывается**: `JoinWebGame()` вызывает только
   `SelectGameModeScreen` (`Src/PW_Client/SelectGameModeScreen.cpp:152-171`), а
   `ClientPW::GotoFirstLobbyScreen()` при активном ctx делает early-return
   (`Src/PW_Client/NewLobbyClientPW.cpp:94-98`) → `ConnectToWebLobby` не шлётся.
3. **IP не ротируется**: все попытки идут на текущий `usedServer`.
4. **Таймауты не влезают в окно резерва** (50 с от момента, когда сервер сам
   обнаружил обрыв): `login_timeout` 30 с
   (`Src/Network/RdpClientTransport/RdpLoginClient.cpp:9`), RDP-handshake на
   мёртвый адрес 10 с (`Src/Network/RUDP/Rdp.cpp:17-19`), паузы попыток 2 с/10 с.
   Клиентские `RdpOptions` не настраиваются вовсе
   (`Src/Network/RdpClientTransport/RdpClientTransport.cpp:142-144`).

## Замер базового поведения (стенд ниже, 2026-10-28)

Обрыв пути клиента к целевому боксу (drop по исходному порту клиента):

| Событие | Момент |
|---|---|
| обрыв (`cutport`) | T+0 |
| клиент: `Excessed retransmit limit. descr=(27310 …:102), try=10` → `ClientChannel: Connection to service failed` → `Lost channel to Game Server` | **T+15 с** |
| сервер: `GameServer: Client disconnected by client (sid=… cid=132)` | T+25 с |
| клиент дальше: только `Cleanup()` (`Closing connection … reason=local`, `[ResourceCollection] destroyed`) — попыток переподключения нет | — |

Итого окно сервера: T+25 с (обнаружил) + 50 с (резерв) = дедлайн ~T+75 с.

## Реализация

### Клиент (`Src/PW_Client/`, `Src/Network/RdpClientTransport/`, `Src/Shared/`)

1. `Shared/ServerIps.{h,cpp}` — нормализованный доступ к индексу пула:
   `GetUsedServer()`, `SetUsedServer(i)`, `NextUsedServer()` (модуло уже делает
   `GetServerIpA`).
2. `PW_Client/FastReconnectCtxPW.h` — политика обхода пула: попытка №1 идёт на
   тот же адрес (обрыв мог быть на стороне клиента), №2..N — остальные адреса
   блока по кругу; старт обхода запоминается в момент обрыва.
3. `PW_Client/GameContext.cpp` — гейт `fast_reconnect_enabled` вместо
   `socialMode` (web-сессия: `g_sessionToken` непустой, бой `Running`, lineup ≥ 2,
   сервер разрешил `IsFastReconnectEnabled()`); перед каждой попыткой —
   `SetUsedServer(pick)` + укороченный login-таймаут; `fast_reconnect_delay`
   2.0→0.5 с, `fast_reconnect_retry_delay` 10.0→1.0 с.
4. `Network/RdpClientTransport/RdpLoginClient.cpp` —
   `rdp_transport::SetLoginOperationTimeout()` (на время реконнекта
   `fast_reconnect_login_timeout` = 3 с вместо 30 с).
5. `Network/RdpClientTransport/RdpClientTransport.cpp` — клиентские
   `RdpOptions` из var'ов: `rdp_cl_ping_period` (5 с), `rdp_cl_retransmit_limit`
   (8), `rdp_cl_handshake_timeout`/`_retry`/`_tries` (3 с / 1 с / 2) — чтобы
   попытка на мёртвый IP стоила секунды, а не десятки секунд.
6. `PW_Client/NewLobbyClientPW.cpp` — в `OnStatusChange(Connected)` при активном
   ctx слать `JoinWebGame(g_protocolToken)` (один раз; серверный путь
   `ConnectToWebLobby` → `ReconnectToCustomGame` уже существует,
   `Src/Game/PF/Server/LobbyPvx/LobbyServerConnection.cpp:309-331`).

### Сервер

Изменений в коде нет (машина резерва/`AddClientFast`/clientctrl целая, покрыта
тестами `Src/Game/PF/HybridServer/Peered.test.cpp`). Возможна только подстройка
cfg (`game_fast_reconnect_reserve_time`, `game_reconnect_timeout`) по итогам
замеров.

### Проксирование

Свой прокси не пишется: прод-контур (DNAT по портам пула) уже используется как
обход blackhole (`pw-net-flood` п.6). Локально воспроизводится теми же
правилами — `tools/pw_local_pool_proxy.sh`.

## Бюджет

| Этап | Значение |
|---|---|
| детект обрыва клиентом | ~10–15 с (замерено 15 с) |
| попытка логина на IP | ≤ 3 с (было до 30 с) |
| обход пула (4–6 адресов) | ≤ 20 с |
| лобби + `ConnectToWebLobby` + `AddClientFast` | ≤ 5 с |
| **итог «зависания»** | **~25–40 с** при окне сервера 75 с |

## Тестовый стенд (локально, прод не трогается)

Два «бокса» на разных IP одной машины + два клиента под Wine:

```
box1 (целевой):  local-gameserver-test/   base 27300  login 10.99.0.2:27301  front 10.99.0.2:27310  gw tcp 34000
box2 (прокси):   local-gameserver-test2/  base 27800  login 10.99.0.3:27801  front 10.99.0.3:27810  gw tcp 34800
мост:            pwpoolbr 10.99.0.1/24 (+ .2, +.3 secondary)
проксирование:   tools/pw_local_pool_proxy.sh setup   # nat OUTPUT DNAT по чужим портам пула
обрыв:           tools/pw_local_pool_proxy.sh cutport <target_ip> <client_sport>   # ACCEPT по ctorigdst=сосед, DROP прямого
клиенты:         tools/pw_reconnect_e2e.sh run        # 2 игрока, WINEPREFIX ~/.wine-pw и ~/.wine-pw2
сессия:          cli/session.js --players 2 --ips 0a6300020a630003
```

Важно: боксы обязаны биндить login/front на **свой** IP
(`login_address`, `frontend_ip_addr` в Profiles) — иначе ответ локального
процесса уходит с адресом `10.99.0.1`, и клиент рвёт соединение по
несовпадению `ConnDescriptor`.

Норма прокси-проб (`tools/pw_vps1_ping.py --once --host <IP> --base <base> --front`):
все 4 пары IP×base дают login `ok type=1`, front `ok type=3`.

Сценарии: T0 бой двух клиентов; T1 обрыв пути одного клиента к целевому, тот же
IP; T2 обрыв — реконнект через чужой IP (прокси); T3 целевой недоступен целиком
→ корректный отказ; T4 повторение без утечек/крашей.

## Ограничение (осознанно не решается)

Соло-сессия (1 игрок): gamesvc завершает бой сразу при `activeClientsCount == 0`
(`Src/Game/PF/HybridServer/PeeredImpl.cpp:739-742`) — реконнект невозможен без
изменения боевой логики. Требование: не делать.
