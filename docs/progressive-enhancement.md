# Legacy HTML4 progressive enhancement

The server-rendered HTML4 form is the canonical move control. Each card, list, or
swimlane move has a stable form ID, an action below validated `ROOT_URL`, and bounded
session, single-use CSRF, operation, object, destination, and optimistic-version
fields. It remains visible and usable when scripts are absent or fail.

The dependency-free enhancement script never examines User-Agent. It first performs
a real `DataTransfer`/`DragEvent` probe and checks the required fetch, abort, form, and
UTF-8 APIs. Only a successful probe binds keyboard and drag events and hides a move
form for which a matching enhanced control and signed form exist. Drag payloads carry
no object IDs or operation names; the matched form remains the only authority.

Enhanced moves use a same-origin credentialed HTTP POST with the form's existing
fields, an exact V1 Accept type, and a monotonic request version. The listener verifies
session, route, operation, one-use CSRF, board scope, target parents, authorization,
optimistic version, and idempotency before its SQLite transaction commits.

## WENA-REGIONS/1

Successful enhancement responses use `application/vnd.wena.regions-v1` and this
bounded byte format:

```text
WENA-REGIONS/1
request-version 12
region board 9 5
Board
end
```

Responses are limited to 32 KiB, eight allowlisted regions, and 4 KiB of strict UTF-8
text per region. Request and region versions must increase. Every region must already
be visible under its renderer-owned stable ID, and names may not repeat. The client
validates the complete response before changing anything, then writes only through
`textContent`; server-provided HTML or script is never interpreted. A response may
therefore update the moved object and other changed regions already visible on the
same page without granting arbitrary DOM access.

## Failure and accessibility

Unsupported capability, malformed or stale data, conflict, replay, timeout, abort,
network failure, unexpected status/media type, missing DOM target, or any partial
validation failure restores all paired HTML4 forms. Ordinary navigation and POST/303
remain available. Enhancement controls are keyboard operable, announce selection,
success, and failure through the live status, transfer focus from a hidden baseline
control, and restore focus to the chosen target after success. The HTML source retains
natural tab order and the baseline buttons, so screen-reader and no-script behavior
does not depend on enhancement initialization.

The `progressive` test suite combines HTML4 rendering, capability/fallback contracts,
bounded region parser/applicator cases, and real-socket HTTP negotiation. SQLite
persistence tests cover card/list/swimlane success, forged scope or parent, optimistic
conflict, replay, rollback, order integrity, and reopen. A real browser-runtime E2E is
still required when a browser or JavaScript DOM runtime is available.
