#ifndef WENA_SERVER_SECURITY_H
#define WENA_SERVER_SECURITY_H

#include <stddef.h>

#define WENA_SECURITY_TOKEN_BYTES 32u
#define WENA_SECURITY_TOKEN_CAPACITY 65u
#define WENA_SECURITY_MAX_SESSIONS 64u
#define WENA_SECURITY_MAX_CSRF 128u
#define WENA_SECURITY_MAX_AUDIT 256u
#define WENA_SECURITY_MAX_SESSION_TTL 86400ul
#define WENA_SECURITY_MAX_CSRF_TTL 900ul

typedef int (*WenaEntropy)(void *context, unsigned char *output, size_t length);

typedef enum WenaSecurityDecision {
    WENA_SECURITY_ACCEPT,
    WENA_SECURITY_REJECT_AUTH,
    WENA_SECURITY_REJECT_EXPIRED,
    WENA_SECURITY_REJECT_REPLAY,
    WENA_SECURITY_REJECT_SCOPE,
    WENA_SECURITY_REJECT_CAPACITY,
    WENA_SECURITY_REJECT_ENTROPY
} WenaSecurityDecision;

typedef struct WenaSession {
    int active;
    char token[WENA_SECURITY_TOKEN_CAPACITY];
    char user_id[65];
    unsigned long expires_at;
} WenaSession;

typedef struct WenaCsrfToken {
    int active;
    int used;
    char token[WENA_SECURITY_TOKEN_CAPACITY];
    char session_token[WENA_SECURITY_TOKEN_CAPACITY];
    char route[257];
    char operation[65];
    unsigned long expires_at;
} WenaCsrfToken;

typedef struct WenaSecurityAudit {
    unsigned long timestamp;
    WenaSecurityDecision decision;
    char user_id[65];
    char route[257];
    char operation[65];
} WenaSecurityAudit;

typedef struct WenaSecurityStore {
    WenaEntropy entropy;
    void *entropy_context;
    WenaSession sessions[WENA_SECURITY_MAX_SESSIONS];
    WenaCsrfToken csrf[WENA_SECURITY_MAX_CSRF];
    WenaSecurityAudit audit[WENA_SECURITY_MAX_AUDIT];
    size_t audit_next;
    size_t audit_count;
} WenaSecurityStore;

void wena_security_init(WenaSecurityStore *store, WenaEntropy entropy, void *context);
int wena_security_session_create(WenaSecurityStore *store, const char *user_id,
                                 unsigned long now, unsigned long ttl,
                                 char *token, size_t capacity);
int wena_security_session_authenticate(WenaSecurityStore *store, const char *token,
                                       unsigned long now, char *user_id,
                                       size_t capacity);
void wena_security_session_revoke(WenaSecurityStore *store, const char *token);
int wena_security_csrf_issue(WenaSecurityStore *store, const char *session_token,
                             const char *route, const char *operation,
                             unsigned long now, unsigned long ttl,
                             char *token, size_t capacity);
int wena_security_csrf_consume(WenaSecurityStore *store, const char *session_token,
                               const char *csrf_token, const char *route,
                               const char *operation, unsigned long now);
const WenaSecurityAudit *wena_security_last_audit(const WenaSecurityStore *store);

#endif
