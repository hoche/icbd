# Network communications (icbd)

This document describes **how `icbd` communicates over the network** (ports, transports, packet framing, session lifecycle, and implementation quirks). It is based on the canonical protocol description in `Protocol.html` and the current server implementation in `server/` and `pktserv/`.

## High-level overview

- **Transport**: TCP.
- **Application protocol**: ICB packets (length-prefixed frames, 1-byte type, `^A` field separators).
- **Listeners**:
  - **Plaintext**: TCP `7326` by default.
  - **TLS** (optional): TCP `7327` by default, on a **separate** listening socket/port.

The server’s event loop is implemented in `pktserv/` (non-blocking sockets + `poll()`), and passes complete ICB packets up into `server/dispatch.c`.

## Ports, listeners, and command-line flags

The main server binary (`server/icbd`) accepts these network-related flags (see `server/main.c`):

- **`-p <port>`**: plaintext listen port (default **7326**).
- **`-s [port]`**: enable TLS listener. If `port` omitted, defaults to **7327**.
- **`-b <host>`**: bind/listen on a specific interface hostname (otherwise binds `INADDR_ANY`).
- **`-f`**: don’t fork (useful for running under a supervisor).

Defaults come from `icb_config.h`:

- `DEFAULT_PORT=7326`
- `DEFAULT_SSLPORT=7327`
- `ICBPEMFILE="./icbd.pem"`

## TLS (SSL) transport

When started with `-s`, the server:

- Initializes an OpenSSL `SSL_CTX` (`pktserv/sslconf.c`)
- Loads **both** the private key and certificate from a single PEM file (`./icbd.pem` by default)
- Listens on the TLS port and performs `SSL_accept()` on new connections (`pktserv/sslsocket.c`)

Operational notes:

- **Client certificates are not required/validated** (`SSL_CTX_set_verify(..., SSL_VERIFY_NONE, ...)`).
- The PEM filename/location is effectively **fixed at runtime** (compiled default `ICBPEMFILE`, and documentation assumes `./icbd.pem` in the server’s working directory). See `README.INSTALL` and `support/mkcert.sh`.

## Packet framing and encoding

### Frame layout

ICB uses a compact, length-prefixed frame:

- **Byte 0**: unsigned length `L` (0–255)
- **Byte 1..L**: payload bytes (includes the 1-byte **packet type** + data)

In this server, packets are treated as C strings after read:

- The read path allocates up to `MAX_PKT_LEN` (256) and, once a full packet is read, appends a `'\0'` for parsing convenience (`pktserv/pktsocket.c`).
- The write path **always includes a trailing NUL** in the payload and includes that NUL in the length (`server/send.c`, `icb_config.h`).

Constants (`icb_config.h`):

- `MAX_PKT_LEN = 256` (includes the length byte)
- `MAX_PKT_DATA = MAX_PKT_LEN - 2` (data bytes excluding length byte and type byte; includes trailing NUL)

### Packet types and fields

- **Packet type** is the first byte of the payload (e.g. `'a'` login, `'b'` open message).
- Many packet payloads contain **fields separated by ASCII `^A`** (`0x01`), per `Protocol.html`.

### Character set

`Protocol.html` describes ASCII payloads.

Implementation note: message text can be UTF-8 if compiled with `ALLOW_UTF8_MESSAGES` (`icb_config.h`), but **nicknames and group names remain restricted**.

## Protocol specification (from `Protocol.html`)

This section incorporates the full contents of `Protocol.html`, organized into a reference-friendly form. Where the current `icbd` implementation differs, see [Implementation quirks / compatibility notes](#protocol-quirks--compatibility-notes).

### Basic packet layout (spec)

Each packet is:

- `L` (1 byte): number of bytes in the packet **excluding** the length byte itself, but **including** the type byte and all packet data.
- `T` (1 byte): packet type (one of the types below).
- `d` (0..254 bytes): packet data (often `^A`-separated fields).

The spec notes an **optional null terminator**: if you include a NUL, it must be included in `L`. The spec “recommends” sending the null terminator for compatibility with older implementations.

**Proposed (spec) extension**: if `L == 0`, treat this packet as part of an extended packet (as if `L == 255`), and append the next packet’s data. (This is a proposal, not universally implemented.)

### Field separation (spec)

When a packet has multiple fields, they are separated by ASCII **`^A`** (`0x01`). Optional trailing fields may be omitted entirely.

### Packet types (spec)

In the tables below:

- **Direction**: which side sends the packet.
- **Fields**: field index → meaning.
- **Layout**: illustrative payload layout (not including the length byte `L`).

#### `'a'` Login / Login OK

- **Client → Server (`'a'` Login)**:
  - **Fields (min 5, max 7)**:
    - 0: login id (required)
    - 1: nickname (required)
    - 2: default group (required; may be empty string for “all groups” in who listing)
    - 3: login command (required): `"login"` or `"w"`
    - 4: password (required, often blank)
    - 5: group status if default group doesn’t exist (optional)
    - 6: protocol level (optional; deprecated)
  - **Layout**:
    - `aLoginid^ANickname^ADefaultGroup^ACommand^APassword^AGroupStatus^AProtocolLevel`
- **Server → Client (`'a'` Login OK)**:
  - **Fields**: none
  - **Layout**: `a`

#### `'b'` Open message

- **Client → Server**:
  - **Fields**: 1
    - 0: message text
  - **Layout**: `bMessageText`
- **Server → Client**:
  - **Fields**: 2
    - 0: sender nickname
    - 1: message text
  - **Layout**: `bNickname^AMessageText`

#### `'c'` Personal message

- **Client → Server**: not valid (per spec).
- **Server → Client**:
  - **Fields**: 2
    - 0: sender nickname
    - 1: message text
  - **Layout**: `cNickname^AMessageText`

#### `'d'` Status message

- **Client → Server**: not valid (per spec).
- **Server → Client**:
  - **Fields**: 2
    - 0: category
    - 1: message text
  - **Layout**: `dCategory^AMessageText`

#### `'e'` Error message

- **Client → Server**: not valid (per spec).
- **Server → Client**:
  - **Fields**: 1
    - 0: message text
  - **Layout**: `eMessageText`

#### `'f'` Important message

- **Client → Server**: not valid (per spec).
- **Server → Client**:
  - **Fields**: 2
    - 0: category
    - 1: message text
  - **Layout**: `fCategory^AMessageText`

#### `'g'` Exit

- **Client → Server**: not valid (per spec).
- **Server → Client**:
  - **Fields**: none
  - **Layout**: `g`

#### `'h'` Command

- **Client → Server**:
  - **Fields (min 1, max 3)**:
    - 0: command (required)
    - 1: arguments (optional)
    - 2: message id (optional)
  - **Layout**: `hCommand^AArguments^AMessageID`
- **Server → Client**: not valid (per spec).

#### `'i'` Command output

- **Client → Server**: not valid (per spec).
- **Server → Client**:
  - **Fields (min 1, max variable)**:
    - 0: output type (required)
    - 1..n-1: output fields (optional; output-type specific)
    - n: message id (optional)
  - **Layout**: `iOutputType^AOutput^AOutput...^AMessageID`

#### `'j'` Protocol

- **Client → Server**:
  - **Fields (min 1, max 3)**:
    - 0: protocol level (required)
    - 1: host id (optional)
    - 2: client id (optional)
  - **Layout**: `jProtoLevel^AHostID^AClientID`
- **Server → Client**:
  - **Fields (min 1, max 3)**:
    - 0: protocol level (required)
    - 1: host id (optional)
    - 2: server id (optional)
  - **Layout**: `jProtoLevel^AHostID^AServerID`

#### `'k'` Beep

- **Client → Server**: not valid (per spec).
- **Server → Client**:
  - **Fields**: 1
    - 0: sender nickname
  - **Layout**: `kNickname`

#### `'l'` Ping

- **Client → Server**:
  - **Fields (min 0, max 1)**:
    - 0: message id (optional)
  - **Layout**: `lMessageID`
- **Server → Client**:
  - **Fields (min 0, max 1)**:
    - 0: message id (optional)
  - **Layout**: `lMessageID`

#### `'m'` Pong

- **Client → Server**:
  - **Fields (min 0, max 1)**:
    - 0: message id (optional)
  - **Layout**: `mMessageID`
- **Server → Client**:
  - **Fields (min 0, max 1)**:
    - 0: message id (optional)
  - **Layout**: `mMessageID`

#### `'n'` No-op

- **Client → Server**:
  - **Fields**: none
  - **Layout**: `n`

### Session lifecycle (spec)

A typical session proceeds as:

- Client opens a connection.
- Server sends a Protocol packet (`'j'`).
- Client sends a Login packet (`'a'`).
- If login command is `"w"`: server sends who listing output and then sends `Exit ('g')`, client closes.
- If login command is `"login"`: server sends Login OK (`'a'`).
- Client and server exchange packets as needed (Open/Personal/Status/Error/Important/Command/Command Output/Beep/Ping/Pong).
- Optionally server sends Exit (`'g'`).
- Client closes.

### Additional spec notes

#### Login packet (spec)

Client can send **one and only one** Login packet.

#### Message IDs for Command/Command Output (spec)

If a client includes a Message ID in a Command packet, output packets produced in response should include the same Message ID.

#### Ping/Pong (spec)

On receiving a Ping packet, the receiver should reply with a Pong. If Ping contains a Message ID, Pong should echo the same ID.

#### Command Output types (spec)

The spec defines these output types:

- **`"co"`**: generic command output
- **`"ec"`**: end of output
- **`"wl"`**: who-listing user line:
  - Field 1: moderator marker (`"*"` or `" "`)
  - Field 2: nickname
  - Field 3: idle seconds
  - Field 4: response time (no longer used)
  - Field 5: login time (Unix `time_t`)
  - Field 6: user id
  - Field 7: host id
  - Field 8: registration status
  - Layout: `iwl^AMod^ANickname^AIdle^AResp^ALoginTime^AUserID^AHostID^ARegisterInfo`
- **`"wg"`**: who-listing group line:
  - Field 1: group name
  - Field 2: group topic
  - Layout: `iwg^AGroupName^AGroupTopic`
- **`"wh"`**: tell client to output header for who listing output (deprecated)
- **`"gh"`**: tell client to output a group header for who listing output (deprecated)
- **`"ch"`**: tell client to list all the commands it handles internally (deprecated)
- **`"c"`**: tell client to list a single command (deprecated)

#### Protocol negotiation (spec)

The spec states there is currently **no way to negotiate** a protocol level; a proposed method may be added later.

## Connection lifecycle (server + client)

### 1) TCP connect

Client opens a TCP connection to the server (plaintext or TLS port).

Socket behavior (see `pktserv/pktsocket.c`, `pktserv/pktserv.c`):

- non-blocking sockets
- `SO_KEEPALIVE` enabled for accepted sockets
- `SO_LINGER` set to “discard immediately” (linger=0)
- `SIGPIPE` ignored
- event loop uses `poll()` with idle callbacks (default `POLL_TIMEOUT=1` second, `icb_config.h`)

### 2) Server sends protocol banner (`'j'`)

On new connection, the server immediately sends a **Protocol** packet:

- Type: `'j'`
- Payload: `j<PROTO_LEVEL>^A<thishost>^A<VERSION>`

See `server/send.c` (`s_new_user()`), `version.h` (`PROTO_LEVEL`), and `Protocol.html`.

### 3) Client sends login (`'a'`)

Client then sends exactly one login packet (type `'a'`) with `^A`-separated fields, per `Protocol.html`.

Important server-side validation (see `server/msgs.c` login handler):

- login id length and allowed characters
- nickname length and uniqueness
- optional deny-list check against `ACCESS_FILE` (`./icbd.deny` by default, wildcard-matched against `LOGINID@HOST`)
- permissions loaded from `PERM_FILE` (`./icbd.perms`), which can:
  - deny login (`PERM_DENY`)
  - throttle read processing (`PERM_SLOWMSGS`)

If login is accepted, server sends:

- Login OK packet (type `'a'`, empty payload beyond type) (`server/send.c:send_loginok()`)
- MOTD and other initial status/command output (implementation-specific)

### 4) Session messaging

After login:

- Client typically sends:
  - `'b'` open messages to group
  - `'h'` commands (server-specific command set)
  - `'n'` no-op keepalive
  - `'m'` pong replies (see note below)
- Server sends:
  - `'b'` open messages (re-broadcast)
  - `'c'` personal messages
  - `'d'` status
  - `'e'` error
  - `'f'` important
  - `'i'` command output
  - `'k'` beep
  - `'l'` ping (see note below)
  - `'g'` exit (when it wants the client to disconnect)

## Command packets (`'h'`) and server commands

Most “slash commands” in ICB are implemented as **Command packets**:

- Type: `'h'`
- Payload (fields separated by `^A`): `hCommand^AArguments^AMessageID`

See `Protocol.html`. On the server side, these are dispatched from `server/msgs.c` (command parsing) through a lookup table (`server/globals.c:command_table`) and implemented across the `server/s_*.c` files (see `server/s_commands.h`).

### What clients receive in response

Server responses are generally one or more of:

- **Status (`'d'`)**: `dCategory^AMessageText` (human-oriented status lines)
- **Error (`'e'`)**: `eMessageText` (command rejected/invalid)
- **Command output (`'i'`)**: `iOutputType^A...` (structured output for `/w`, `/help`, etc.)
  - Common output types include `"co"` (generic output) and `"ec"` (end of command output) (see `Protocol.html` and `server/send.c`).

### Key commands worth documenting (network-visible behavior)

This list is not exhaustive, but covers the commands that most directly affect network-visible behavior and server state.

#### `/w` (who)

Implemented by `server/s_who.c`.

- Supports flags like `-g` (groups-only) and `-s` (short) and targets:
  - empty: list all groups
  - `.`: your current group
  - `@nickname`: who for that user’s group (subject to group visibility rules)
- Output is sent using **Command Output (`'i'`)** packets (e.g. `"wl"` user lines and `"wg"` group lines), plus `"ec"` end markers (see `Protocol.html`).

#### `/topic`

Implemented by `server/s_group.c:s_topic()`.

- Reads or sets a group topic.
- When setting, the server broadcasts a **Status (`'d'`)** message to the group via `s_status_group(..., "Topic", ...)`.

#### `/g` (group change)

Implemented by `server/s_group.c:s_change()`.

- Moves the user to a different group and triggers “arrive/leave” style **Status (`'d'`)** messages to affected users/groups.

#### `/ping <nick>`

Implemented by `server/s_user.c:s_ping()` and `server/msgs.c:pong()`.

- Causes the server to send an ICB **PING packet (`'l'`)** to the target user.
- When the target client replies with **PONG (`'m'`)**, the server reports RTT back to the requester via a **Status (`'d'`)** message with category `"PONG"`.

#### `/status ...` (group “set” / group status commands)

Implemented by `server/s_group.c:s_status()`; option tokens are defined in `server/globals.c:status_table`.

`/status` accepts one or more option tokens (and, for some tokens, an argument). Many options result in a group-broadcast **Status (`'d'`)** message with category `"Change"` via `s_status_group(...)`.

Common options:

- **Control**: `r` (restricted), `m` (moderated), `c` (controlled), `p` (public)
- **Visibility**: `i` (invisible / supersecret), `s` (secret), `v` (visible)
- **Volume**: `q` (quiet), `n` (normal), `l` (loud)
- **Group name**: `name <newname>`
- **Group size**: `# <maxUsers>` (0 means unlimited)

Idle-related options (your request):

- **Idle boot time**: `b <minutes>`
  - Sets the per-group idle-boot threshold in minutes (`server/s_group.c`, `SET_IDLEBOOT`).
  - `b 0` disables idle-boot for the group.
  - If `MAX_IDLE` is compiled in, the value is bounded by that global idle limit; otherwise it must simply be non-negative.
- **Idle boot message**: `idlebootmsg <message>`
  - Sets the message used when users are idle-booted from the group (`SET_IDLEBOOT_MSG`).
  - This message is validated to allow only a limited number of format substitutions and is length-limited by `IDLEBOOT_MSG_LEN`.
  - Unlike most `/status` changes, the server responds **only to the requester** with a **Status (`'d'`)** message category `"Change"` containing the new message (it is not group-broadcast).
- **Idle mod timeout**: `im <minutes>`
  - Sets how long a moderator can be idle before moderatorship is dislodged (`SET_IDLEMOD`), bounded by `MAX_IDLE_MOD`.

Policy/permissions notes:

- Some `/status` changes are refused for groups without a moderator (and the server will emit an **Error (`'e'`)**) — this includes `b` (idleboot) and `im` (idlemod) in addition to some control/volume transitions (`server/s_group.c`).
- Some changes require the requester to be the moderator when a moderator exists.

### What “idleboot” does at runtime

Idle-boot is enforced during the server’s periodic maintenance loop (`server/ipcf.c:s_didpoll()`):

- For each logged-in user, if their group has `idleboot > 0`, and they’ve been idle longer than that threshold, and they are not already in the special idle group, the server:
  - sends the user a **Status (`'d'`)** message category `"Idle-Boot"`
  - optionally announces to the group (depending on volume) via group status
  - moves the user to the special idle group `IDLE_GROUP` (default name `~IDLE~`, `icb_config.h`)

The default idle-boot message is `IDLE_BOOT_MSG` in `icb_config.h`, and can be overridden per-group via `/status idlebootmsg ...`.

## Ping/Pong and keepalives (implementation notes)

`Protocol.html` allows both sides to send `PING ('l')` and reply with `PONG ('m')`.

**This server differs**:

- It **does not accept inbound `PING ('l')`** from clients; receiving one triggers an error response (`server/dispatch.c`).
- It **does handle inbound `PONG ('m')`** and uses it to report round-trip time for the `/ping` command (`server/msgs.c:pong()`).
- The server itself sends `PING ('l')` frames using `server/send.c:sendping()`.

For keepalive without affecting idle time, clients can send **`NOOP ('n')`** (server ignores it; `server/dispatch.c`).

## Timeouts, disconnects, and throttling

- **Idle disconnect**: optionally compiled via `MAX_IDLE` (disabled by default in `icb_config.h` via `#undef MAX_IDLE`). When enabled, the server warns `IDLE_WARN` seconds before dropping (`server/ipcf.c` / `server/msgs.c`).
- **Disconnect semantics**:
  - Server may send `EXIT ('g')` and then close.
  - Server may also close immediately on protocol/IO errors or policy decisions (deny-list, oversized packets, etc.).
- **Oversized packets**: if the peer claims a length > `(MAX_PKT_LEN-1)` the connection is terminated (`pktserv/pktsocket.c`).
- **Read throttling**: `PERM_SLOWMSGS` causes the poll loop to skip reads if the user sent data within the last second (`server/msgs.c:ok2read()`).

## Name resolution and anti-spoofing

The server resolves client identity for logging and policy:

- Retrieves peer IP with `getpeername()`
- Performs reverse DNS and (by default) a **double-reverse check** (PTR then forward lookup must include the original IP) (`pktserv/getrname.c`)
- If reverse lookup fails, uses bracketed IP string like `"[1.2.3.4]"`.

You can disable the double-reverse behavior at compile time with `NO_DOUBLE_RES_LOOKUPS` (`icb_config.h`).

## Protocol quirks / compatibility notes

- `Protocol.html` proposes an “extended packet” scheme where `L==0` means the packet is continued in subsequent frames. **This server does not implement that extension**; a length byte of 0 can lead to immediate disconnect behavior in the current read loop (`pktserv/pktsocket.c`).
- The server generally expects to parse payloads as NUL-terminated C strings; it ensures NUL termination after reading a frame, and it **always** includes a trailing NUL in frames it sends (`icb_config.h`, `server/send.c`).

## Where to look in the source

- **ICB protocol reference**: `Protocol.html`
- **Packet types**: `server/protocol.h`
- **Listener setup / ports / flags**: `server/main.c`, `icb_config.h`
- **Event loop and socket behavior**: `pktserv/pktserv.c`, `pktserv/pktsocket.c`
- **TLS support**: `pktserv/sslconf.c`, `pktserv/sslsocket.c`
- **Server handshake** (sends `'j'`): `server/send.c:s_new_user()`
- **Packet dispatch**: `server/dispatch.c`, `server/ipcf.c:s_packet()`
- **Login policy** (deny/perms): `server/msgs.c` (`ACCESS_FILE`, `PERM_FILE`)

