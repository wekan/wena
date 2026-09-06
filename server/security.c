#include "security.h"

#include <string.h>

static int wena_copy(char *destination, size_t capacity, const char *source)
{
    size_t length;
    if (destination == NULL || capacity == 0 || source == NULL) return 0;
    length = strlen(source);
    if (length == 0 || length >= capacity) return 0;
    memcpy(destination, source, length + 1);
    return 1;
}

static int wena_token_equal(const char *left, const char *right)
{
    unsigned int difference;
    size_t index;
    if (left == NULL || right == NULL || strlen(left) != 64 || strlen(right) != 64)
        return 0;
    difference = 0u;
    for (index = 0; index < 64; ++index)
        difference |= (unsigned int)((unsigned char)left[index] ^ (unsigned char)right[index]);
    return difference == 0u;
}

static int wena_token_exists(WenaSecurityStore *store, const char *token)
{
    size_t index;
    for (index = 0; index < WENA_SECURITY_MAX_SESSIONS; ++index)
        if (store->sessions[index].active && wena_token_equal(store->sessions[index].token, token))
            return 1;
    for (index = 0; index < WENA_SECURITY_MAX_CSRF; ++index)
        if (store->csrf[index].active && wena_token_equal(store->csrf[index].token, token))
            return 1;
    return 0;
}

static int wena_token(WenaSecurityStore *store, char *output)
{
    static const char hex[] = "0123456789abcdef";
    unsigned char bytes[WENA_SECURITY_TOKEN_BYTES];
    size_t index;
    unsigned int any;
    int attempt;
    for (attempt = 0; attempt < 3; ++attempt) {
        if (store->entropy == NULL ||
            !store->entropy(store->entropy_context, bytes, sizeof(bytes))) return 0;
        any = 0u;
        for (index = 0; index < sizeof(bytes); ++index) {
            any |= bytes[index];
            output[index * 2] = hex[bytes[index] >> 4];
            output[index * 2 + 1] = hex[bytes[index] & 15];
        }
        output[64] = '\0';
        if (any != 0u && !wena_token_exists(store, output)) return 1;
    }
    return 0;
}

static void wena_audit(WenaSecurityStore *store, unsigned long now,
                       WenaSecurityDecision decision, const char *user,
                       const char *route, const char *operation)
{
    WenaSecurityAudit *entry;
    entry = &store->audit[store->audit_next];
    memset(entry, 0, sizeof(*entry));
    entry->timestamp = now;
    entry->decision = decision;
    if (user != NULL) wena_copy(entry->user_id, sizeof(entry->user_id), user);
    if (route != NULL) wena_copy(entry->route, sizeof(entry->route), route);
    if (operation != NULL) wena_copy(entry->operation, sizeof(entry->operation), operation);
    store->audit_next = (store->audit_next + 1) % WENA_SECURITY_MAX_AUDIT;
    if (store->audit_count < WENA_SECURITY_MAX_AUDIT) ++store->audit_count;
}

void wena_security_init(WenaSecurityStore *store, WenaEntropy entropy, void *context)
{
    if (store != NULL) {
        memset(store, 0, sizeof(*store));
        store->entropy = entropy;
        store->entropy_context = context;
    }
}

static WenaSession *wena_session(WenaSecurityStore *store, const char *token,
                                 unsigned long now, WenaSecurityDecision *decision)
{
    size_t index;
    for (index = 0; index < WENA_SECURITY_MAX_SESSIONS; ++index) {
        if (store->sessions[index].active &&
            wena_token_equal(store->sessions[index].token, token)) {
            if (store->sessions[index].expires_at <= now) {
                store->sessions[index].active = 0;
                *decision = WENA_SECURITY_REJECT_EXPIRED;
                return NULL;
            }
            return &store->sessions[index];
        }
    }
    *decision = WENA_SECURITY_REJECT_AUTH;
    return NULL;
}

int wena_security_session_create(WenaSecurityStore *store, const char *user_id,
                                 unsigned long now, unsigned long ttl,
                                 char *token, size_t capacity)
{
    size_t index;
    char generated[WENA_SECURITY_TOKEN_CAPACITY];
    if (store == NULL || token == NULL || capacity < WENA_SECURITY_TOKEN_CAPACITY ||
        user_id == NULL || user_id[0] == '\0' || strlen(user_id) >= 65 || ttl == 0 ||
        ttl > WENA_SECURITY_MAX_SESSION_TTL || now > ~0ul - ttl) return 0;
    for (index = 0; index < WENA_SECURITY_MAX_SESSIONS; ++index)
        if (!store->sessions[index].active || store->sessions[index].expires_at <= now) break;
    if (index == WENA_SECURITY_MAX_SESSIONS) {
        wena_audit(store, now, WENA_SECURITY_REJECT_CAPACITY, user_id, NULL, NULL);
        return 0;
    }
    if (!wena_token(store, generated)) {
        wena_audit(store, now, WENA_SECURITY_REJECT_ENTROPY, user_id, NULL, NULL);
        return 0;
    }
    memset(&store->sessions[index], 0, sizeof(store->sessions[index]));
    store->sessions[index].active = 1;
    strcpy(store->sessions[index].token, generated);
    strcpy(store->sessions[index].user_id, user_id);
    store->sessions[index].expires_at = now + ttl;
    strcpy(token, generated);
    wena_audit(store, now, WENA_SECURITY_ACCEPT, user_id, NULL, NULL);
    return 1;
}

int wena_security_session_authenticate(WenaSecurityStore *store, const char *token,
                                       unsigned long now, char *user_id,
                                       size_t capacity)
{
    WenaSecurityDecision decision;
    WenaSession *session;
    if (store == NULL) return 0;
    decision = WENA_SECURITY_REJECT_AUTH;
    session = wena_session(store, token, now, &decision);
    if (session == NULL || !wena_copy(user_id, capacity, session->user_id)) {
        wena_audit(store, now, decision, NULL, NULL, NULL);
        return 0;
    }
    wena_audit(store, now, WENA_SECURITY_ACCEPT, session->user_id, NULL, NULL);
    return 1;
}

void wena_security_session_revoke(WenaSecurityStore *store, const char *token)
{
    size_t index;
    if (store == NULL) return;
    for (index = 0; index < WENA_SECURITY_MAX_SESSIONS; ++index)
        if (store->sessions[index].active && wena_token_equal(store->sessions[index].token, token))
            store->sessions[index].active = 0;
}

int wena_security_csrf_issue(WenaSecurityStore *store, const char *session_token,
                             const char *route, const char *operation,
                             unsigned long now, unsigned long ttl,
                             char *token, size_t capacity)
{
    WenaSecurityDecision decision;
    WenaSession *session;
    size_t index;
    char generated[WENA_SECURITY_TOKEN_CAPACITY];
    if (store == NULL || token == NULL || capacity < WENA_SECURITY_TOKEN_CAPACITY ||
        route == NULL || route[0] != '/' || strlen(route) >= 257 || operation == NULL ||
        operation[0] == '\0' || strlen(operation) >= 65 || ttl == 0 ||
        ttl > WENA_SECURITY_MAX_CSRF_TTL || now > ~0ul - ttl) return 0;
    decision = WENA_SECURITY_REJECT_AUTH;
    session = wena_session(store, session_token, now, &decision);
    if (session == NULL) {
        wena_audit(store, now, decision, NULL, route, operation);
        return 0;
    }
    for (index = 0; index < WENA_SECURITY_MAX_CSRF; ++index)
        if (!store->csrf[index].active || store->csrf[index].used ||
            store->csrf[index].expires_at <= now) break;
    if (index == WENA_SECURITY_MAX_CSRF || !wena_token(store, generated)) {
        wena_audit(store, now, index == WENA_SECURITY_MAX_CSRF ?
                   WENA_SECURITY_REJECT_CAPACITY : WENA_SECURITY_REJECT_ENTROPY,
                   session->user_id, route, operation);
        return 0;
    }
    memset(&store->csrf[index], 0, sizeof(store->csrf[index]));
    store->csrf[index].active = 1;
    strcpy(store->csrf[index].token, generated);
    strcpy(store->csrf[index].session_token, session_token);
    strcpy(store->csrf[index].route, route);
    strcpy(store->csrf[index].operation, operation);
    store->csrf[index].expires_at = now + ttl;
    strcpy(token, generated);
    wena_audit(store, now, WENA_SECURITY_ACCEPT, session->user_id, route, operation);
    return 1;
}

int wena_security_csrf_consume(WenaSecurityStore *store, const char *session_token,
                               const char *csrf_token, const char *route,
                               const char *operation, unsigned long now)
{
    WenaSecurityDecision decision;
    WenaSession *session;
    size_t index;
    if (store == NULL) return 0;
    decision = WENA_SECURITY_REJECT_AUTH;
    session = wena_session(store, session_token, now, &decision);
    if (session == NULL) {
        wena_audit(store, now, decision, NULL, route, operation);
        return 0;
    }
    for (index = 0; index < WENA_SECURITY_MAX_CSRF; ++index) {
        WenaCsrfToken *csrf;
        csrf = &store->csrf[index];
        if (!csrf->active || !wena_token_equal(csrf->token, csrf_token)) continue;
        if (csrf->used) decision = WENA_SECURITY_REJECT_REPLAY;
        else if (csrf->expires_at <= now) decision = WENA_SECURITY_REJECT_EXPIRED;
        else if (!wena_token_equal(csrf->session_token, session_token) || route == NULL ||
                 operation == NULL || strcmp(csrf->route, route) != 0 ||
                 strcmp(csrf->operation, operation) != 0) decision = WENA_SECURITY_REJECT_SCOPE;
        else decision = WENA_SECURITY_ACCEPT;
        csrf->used = 1;
        wena_audit(store, now, decision, session->user_id, route, operation);
        return decision == WENA_SECURITY_ACCEPT;
    }
    wena_audit(store, now, WENA_SECURITY_REJECT_AUTH, session->user_id, route, operation);
    return 0;
}

const WenaSecurityAudit *wena_security_last_audit(const WenaSecurityStore *store)
{
    size_t index;
    if (store == NULL || store->audit_count == 0) return NULL;
    index = (store->audit_next + WENA_SECURITY_MAX_AUDIT - 1) % WENA_SECURITY_MAX_AUDIT;
    return &store->audit[index];
}
