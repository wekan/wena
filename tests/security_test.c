#include "../server/security.h"

#include <assert.h>
#include <string.h>

typedef struct TestEntropy {
    unsigned int next;
    int fail;
    int zeros;
} TestEntropy;

static int test_entropy(void *context, unsigned char *output, size_t length)
{
    TestEntropy *entropy;
    size_t index;
    entropy = (TestEntropy *)context;
    if (entropy->fail) return 0;
    ++entropy->next;
    for (index = 0; index < length; ++index)
        output[index] = entropy->zeros ? 0 : (unsigned char)(entropy->next + index);
    return 1;
}

int main(void)
{
    WenaSecurityStore store;
    TestEntropy entropy;
    char session[WENA_SECURITY_TOKEN_CAPACITY];
    char other_session[WENA_SECURITY_TOKEN_CAPACITY];
    char csrf[WENA_SECURITY_TOKEN_CAPACITY];
    char scoped[WENA_SECURITY_TOKEN_CAPACITY];
    char user[65];
    const WenaSecurityAudit *audit;
    size_t index;

    memset(&entropy, 0, sizeof(entropy));
    wena_security_init(&store, test_entropy, &entropy);
    assert(wena_security_session_create(&store, "user-1", 100ul, 3600ul,
                                        session, sizeof(session)));
    assert(strlen(session) == 64);
    audit = wena_security_last_audit(&store);
    assert(audit != NULL && audit->decision == WENA_SECURITY_ACCEPT);
    assert(wena_security_session_authenticate(&store, session, 101ul, user, sizeof(user)));
    assert(strcmp(user, "user-1") == 0);
    assert(!wena_security_session_authenticate(&store,
           "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
           101ul, user, sizeof(user)));
    assert(wena_security_last_audit(&store)->decision == WENA_SECURITY_REJECT_AUTH);

    assert(wena_security_csrf_issue(&store, session, "/b/one/demo", "create-card",
                                    102ul, 60ul, csrf, sizeof(csrf)));
    assert(wena_security_csrf_consume(&store, session, csrf, "/b/one/demo",
                                      "create-card", 103ul));
    assert(wena_security_last_audit(&store)->decision == WENA_SECURITY_ACCEPT);
    assert(!wena_security_csrf_consume(&store, session, csrf, "/b/one/demo",
                                       "create-card", 104ul));
    assert(wena_security_last_audit(&store)->decision == WENA_SECURITY_REJECT_REPLAY);

    assert(wena_security_csrf_issue(&store, session, "/b/one/demo", "archive-card",
                                    105ul, 60ul, scoped, sizeof(scoped)));
    assert(!wena_security_csrf_consume(&store, session, scoped, "/b/other/demo",
                                       "archive-card", 106ul));
    assert(wena_security_last_audit(&store)->decision == WENA_SECURITY_REJECT_SCOPE);
    assert(!wena_security_csrf_consume(&store, session, scoped, "/b/one/demo",
                                       "archive-card", 107ul));
    assert(wena_security_last_audit(&store)->decision == WENA_SECURITY_REJECT_REPLAY);

    assert(wena_security_session_create(&store, "user-2", 108ul, 100ul,
                                        other_session, sizeof(other_session)));
    assert(wena_security_csrf_issue(&store, session, "/b/one/demo", "archive-card",
                                    109ul, 2ul, scoped, sizeof(scoped)));
    assert(!wena_security_csrf_consume(&store, other_session, scoped, "/b/one/demo",
                                       "archive-card", 110ul));
    assert(wena_security_last_audit(&store)->decision == WENA_SECURITY_REJECT_SCOPE);
    assert(wena_security_csrf_issue(&store, session, "/b/one/demo", "archive-card",
                                    111ul, 1ul, scoped, sizeof(scoped)));
    assert(!wena_security_csrf_consume(&store, session, scoped, "/b/one/demo",
                                       "archive-card", 112ul));
    assert(wena_security_last_audit(&store)->decision == WENA_SECURITY_REJECT_EXPIRED);

    wena_security_session_revoke(&store, session);
    assert(!wena_security_session_authenticate(&store, session, 113ul, user, sizeof(user)));
    assert(!wena_security_session_create(&store, "", 1ul, 1ul, session, sizeof(session)));
    assert(!wena_security_session_create(&store, "user", 1ul, 0ul, session, sizeof(session)));
    assert(!wena_security_session_create(&store, "user", 1ul,
                                         WENA_SECURITY_MAX_SESSION_TTL + 1ul,
                                         session, sizeof(session)));

    entropy.fail = 1;
    assert(!wena_security_session_create(&store, "user", 200ul, 1ul,
                                         session, sizeof(session)));
    assert(wena_security_last_audit(&store)->decision == WENA_SECURITY_REJECT_ENTROPY);
    entropy.fail = 0;
    entropy.zeros = 1;
    assert(!wena_security_session_create(&store, "user", 200ul, 1ul,
                                         session, sizeof(session)));
    assert(wena_security_last_audit(&store)->decision == WENA_SECURITY_REJECT_ENTROPY);

    entropy.zeros = 0;
    wena_security_init(&store, test_entropy, &entropy);
    for (index = 0; index < WENA_SECURITY_MAX_SESSIONS; ++index)
        assert(wena_security_session_create(&store, "capacity-user", 300ul, 100ul,
                                            session, sizeof(session)));
    assert(!wena_security_session_create(&store, "overflow", 300ul, 100ul,
                                         session, sizeof(session)));
    assert(wena_security_last_audit(&store)->decision == WENA_SECURITY_REJECT_CAPACITY);

    wena_security_init(&store, test_entropy, &entropy);
    assert(wena_security_session_create(&store, "csrf-capacity", 400ul, 1000ul,
                                        session, sizeof(session)));
    for (index = 0; index < WENA_SECURITY_MAX_CSRF; ++index)
        assert(wena_security_csrf_issue(&store, session, "/b/one", "create-card",
                                        400ul, 100ul, csrf, sizeof(csrf)));
    assert(!wena_security_csrf_issue(&store, session, "/b/one", "create-card",
                                     400ul, 100ul, csrf, sizeof(csrf)));
    assert(wena_security_last_audit(&store)->decision == WENA_SECURITY_REJECT_CAPACITY);
    assert(!wena_security_csrf_issue(&store, session, "not-a-route", "create-card",
                                     400ul, 1ul, csrf, sizeof(csrf)));
    assert(!wena_security_csrf_issue(&store, session, "/b/one", "create-card",
                                     400ul, WENA_SECURITY_MAX_CSRF_TTL + 1ul,
                                     csrf, sizeof(csrf)));
    for (index = 0; index < WENA_SECURITY_MAX_AUDIT + 20u; ++index)
        wena_security_session_authenticate(&store, "bad", 301ul, user, sizeof(user));
    assert(store.audit_count == WENA_SECURITY_MAX_AUDIT);
    return 0;
}
