# Models

Shared board, swimlane, list, card, checklist, user, and activity data structures
belong here. As in Meteor WeKan, models may be used by both client and server code
and must not depend on server-only modules.

`card_people` shares stable ID-set operations between card members and assignees.
A complete board roster determines add eligibility; removal can clean up departed
members. This follows the pinned WeKan `setAccessibleCardPerson` and
`canAssignCardMember` behavior. The transfer helper separately follows
`Cards.move`: keep only members active on the destination board, preserving their
order. It does not infer the transfer policy of other person fields.

These pure models neither authenticate an actor nor authorize a write. Storage
must capture roster/card revisions in the same transaction and enforce the
caller's authorization before applying the plan. Board and card person sets are
bounded at 2048 IDs; callers should allocate the large structures on the heap.
Failures preserve output, including the changed flag; repeated actions are
successful no-ops. Persistence and native member controls remain roadmap work.
