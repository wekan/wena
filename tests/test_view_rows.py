#!/usr/bin/env python3
"""Wena's other board views against WeKan's own code.

One seeded board is given to WeKan's tableViewSort.js, chartCalculations.js,
boardTimeline.js, scrumCardOrder.js and scrum.js in Node - each replayed the
way its view calls it (tableView.js, timeView.js, groupByAssigneeView.js,
timelineView.js, scrumView.js) - and to Wena's models/view_rows.c. The Table's
order for every sortable column, both directions, grouped or not and with
searches; the Time view's sums; the assignee groups; the Timeline's markers
and its cards as they were at three points in time; the Scrum order and
estimates must be the same. The Calendar's and the Gantt's rules - plain
selectors in WeKan (calendarFilter.js, frappeGantt.js cardsToTasks) - are
replayed in the same harness. Skipped without a WeKan checkout or Node.
"""
import json
import os
from pathlib import Path
import random
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
WEKAN = Path(os.environ.get("WEKAN_ROOT", ROOT.parent.parent))
DAY = 86400000
NOW = 1790000000000 + 13 * 3600000
FIELD_KEYS = ["title", "listTitle", "swimlaneTitle", "assigneesKey", "labelsKey", "startAt", "dueAt", "endAt"]
FIELD_IDS = [0, 1, 2, 3, 5, 7, 8, 9]   # WenaTableField
QUERIES = ["", "card 1", "  REVIEW ", "nothing"]

HARNESS = r"""
const path = require('path');
const fs = require('fs');
const [root, fixturePath, nowText] = process.argv.slice(1);
const NOW = Number(nowText);
const RealDate = Date;
global.Date = class extends RealDate {
  constructor(...args) { if (args.length === 0) super(NOW); else super(...args); }
  static now() { return NOW; }
};
const { compareTableViewRows } = require(path.join(root, 'models/lib/tableViewSort'));
const c = require(path.join(root, 'models/lib/chartCalculations'));
const { reconstructBoardStateAt } = require(path.join(root, 'models/lib/boardTimeline.js'));
const { compareScrumCards } = require(path.join(root, 'models/lib/scrumCardOrder'));
const { getCardEstimate } = require(path.join(root, 'models/lib/scrum'));
const fx = JSON.parse(fs.readFileSync(fixturePath, 'utf8'));
const date = v => v == null ? undefined : new Date(v);
const users = Object.fromEntries(fx.users.map(u => [u.id, u.name]));
const nameOf = id => users[id] || id;
const cards = fx.cards.map(k => ({ _id: k.id, title: k.title, listId: k.list, swimlaneId: k.swimlane, boardId: 'b',
  createdAt: date(k.created), archived: !!k.archived, archivedAt: date(k.archivedAt), startAt: date(k.start),
  endAt: date(k.end), dueAt: date(k.due), receivedAt: date(k.received), spentTime: k.spent, isOvertime: !!k.overtime,
  assignees: k.assignees, members: k.members, labelIds: k.labels, description: k.description, sort: k.sort,
  poker: k.poker == null ? undefined : { estimation: k.poker },
  customFields: k.estField == null ? [] : [{ _id: 'est', value: k.estField }],
  scrum: { sprintId: k.sprint || null, backlogRank: k.rank == null ? null : k.rank, releaseId: k.release || null } }));
const lists = Object.fromEntries(fx.lists.map(l => [l.id, l]));
const lanes = Object.fromEntries(fx.swimlanes.map(s => [s.id, s]));
const labels = Object.fromEntries(fx.labels.map(l => [l.id, l]));
const out = { table: {}, calendar: [], time: null, groups: [], markers: [], timeline: [], gantt: [], scrum: [], estimate: {} };
// Table: tableView.js's rows, its search and its sort.
const base = cards.filter(k => !k.archived).sort((a, b) => a.title < b.title ? -1 : a.title > b.title ? 1 : 0);
const rows = [];
base.forEach(card => {
  const lane = lanes[card.swimlaneId], list = lists[card.listId];
  if (!lane || !list) return;
  const ls = (card.labelIds || []).map(id => labels[id]).filter(Boolean).map(l => ({ name: l.name || '', color: l.color }));
  rows.push({ card, title: card.title || '', listTitle: list.title || '', swimlaneTitle: lane.title || '',
    swimlaneId: lane.id, swimlaneSort: lane.sort || 0, startAt: card.startAt || null, dueAt: card.dueAt || null,
    endAt: card.endAt || null, receivedAt: card.receivedAt || null, labels: ls,
    assigneesKey: (card.assignees || []).join(' '), membersKey: (card.members || []).join(' '),
    labelsKey: ls.map(l => l.name).join(' ') });
});
for (const [fi, field] of JSON.parse(fx.fields).entries()) {
  for (const dir of ['asc', 'desc']) for (const group of [false, true]) for (const raw of JSON.parse(fx.queries)) {
    const query = raw.trim().toLowerCase();
    let filtered = query ? rows.filter(r => [r.title, r.listTitle, r.swimlaneTitle, ...r.labels.map(l => l.name)]
      .join(' ').toLowerCase().indexOf(query) !== -1) : rows;
    filtered = filtered.slice().sort((a, b) => {
      if (group) {
        const lo = a.swimlaneSort - b.swimlaneSort; if (lo !== 0) return lo;
        const lt = a.swimlaneTitle.localeCompare(b.swimlaneTitle); if (lt !== 0) return lt;
        const li = a.swimlaneId.localeCompare(b.swimlaneId); if (li !== 0) return li;
      }
      return compareTableViewRows(a, b, field, dir);
    });
    out.table[`${fi}|${dir === 'desc' ? 1 : 0}|${group ? 1 : 0}|${raw}`] = filtered.map(r => r.card._id);
  }
}
// Calendar: calendarFilter.js's selectors, an hour for a date, by id.
const from = new Date(NOW - 40 * 86400000), to = new Date(NOW - 10 * 86400000);
const ev = [];
cards.forEach(k => {
  if (k.startAt && k.endAt && ((k.startAt <= from && k.endAt >= from) || (k.startAt <= to && k.endAt >= to) || (k.startAt >= from && k.endAt <= to)))
    ev.push([k._id, 0, +k.startAt, +k.endAt]);
  if (k.receivedAt && k.receivedAt >= from && k.receivedAt <= to) ev.push([k._id, 1, +k.receivedAt, +k.receivedAt + 36e5]);
  if (k.dueAt && k.dueAt >= from && k.dueAt <= to) ev.push([k._id, 2, +k.dueAt, +k.dueAt + 36e5]);
  if (k.endAt && k.endAt >= from && k.endAt <= to) ev.push([k._id, 3, +k.endAt, +k.endAt + 36e5]);
});
ev.sort((a, b) => a[0] > b[0] ? 1 : -1);
out.calendar = ev;
// Time: boardChartData.js's 'time'.
const active = cards.filter(k => !k.archived);
const rem = c.computeRemainingTimeSum(active, new Date(NOW));
out.time = { days: rem.days, hours: rem.hours, count: rem.cardCount, withTime: active.filter(k => (Number(k.spentTime) || 0) > 0).length,
  byAssignee: c.computeTimeByGroup(active, k => (k.assignees || []).map(id => ({ key: id, label: nameOf(id) })),
    { key: '__no_assignee__', label: 'No assignee' }).map(g => [g.label, g.hours, g.cards]),
  byCard: c.computeTimeByCard(active).map(k => k.key) };
// Group by Assignee.
out.groups = c.computeCardsByAssigneeGroup(active, k => (k.assignees || []).map(id => ({ key: id, label: nameOf(id) })),
  { key: '__no_assignee__', label: 'No assignee' }).map(g => [g.label, ...g.cards.map(k => k.cardId)]);
// Timeline: timelineView.js's markers and reconstructBoardStateAt.
const acts = fx.activities.map(a => ({ activityType: a.type, cardId: a.card, listId: a.list, createdAt: new Date(a.at),
  ...(a.hasOldList ? { oldListId: a.old } : {}), ...(a.hasOldLane ? { oldSwimlaneId: a.oldLane } : {}),
  memberId: a.member || undefined, labelId: a.label || undefined, oldValue: a.oldValue || undefined,
  timeKey: a.timeKey || undefined, timeOldValue: a.timeOld == null ? undefined : new Date(a.timeOld) }));
const times = Array.from(new Set(acts.map(a => a.createdAt.getTime()).filter(t => !!t))).sort((a, b) => a - b);
let sampled = times;
if (times.length > 50) { const step = times.length / 50; sampled = []; for (let i = 0; i < 50; i++) sampled.push(times[Math.floor(i * step)]); }
out.markers = sampled;
const live = cards.filter(k => !k.archived);
for (const q of [0, 1, 2]) {
  const states = q === 0 ? live.map(k => ({ _id: k._id, existed: true, title: k.title, listId: k.listId, archived: !!k.archived,
    dueAt: k.dueAt || null, labelIds: k.labelIds || [], members: k.members || [] }))
    : reconstructBoardStateAt(live, acts, new Date(NOW - q * 20 * 86400000)).cards;
  states.forEach(s => out.timeline.push([q, s._id, s.existed ? 1 : 0, s.title, s.listId, s.archived ? 1 : 0,
    s.dueAt ? String(+new Date(s.dueAt)) : '-', (s.labelIds || []).join(','), (s.members || []).join(',')]));
}
// Gantt: frappeGantt.js cardsToTasks.
const iso = v => new Date(v).toISOString().slice(0, 10);
const today = iso(new Date());
cards.forEach(k => {
  const sf = k.startAt ? 'startAt' : (k.receivedAt ? 'receivedAt' : null);
  if (!sf) return;
  const start = iso(k[sf]);
  const ef = k.dueAt ? 'dueAt' : (k.endAt ? 'endAt' : null);
  let end = ef ? iso(k[ef]) : null;
  if (!end || end <= start) { const d = new Date(`${start}T00:00:00.000Z`); d.setUTCDate(d.getUTCDate() + 1); end = iso(d); }
  out.gantt.push([k._id, Date.parse(`${start}T00:00:00Z`), Date.parse(`${end}T00:00:00Z`), k.endAt ? 1 : 0,
    k.dueAt && !k.endAt && iso(k.dueAt) < today ? 1 : 0]);
});
// Scrum: scrumView.js viewCards and getCardEstimate.
for (const sprint of ['', 'sp1'])
  out.scrum.push(cards.filter(k => !k.archived && (sprint ? k.scrum.sprintId === sprint : !k.scrum.sprintId)).sort(compareScrumCards).map(k => k._id));
const settings = fx.estimateField ? { estimateSource: 'customField', estimateCustomFieldId: 'est' } : { estimateSource: 'poker' };
cards.forEach(k => { const e = getCardEstimate(k, settings); out.estimate[k._id] = e === null ? '-' : String(Math.round(e * 100) / 100); });
process.stdout.write(JSON.stringify(out));
"""


def fixture(seed, estimate_field):
    rng = random.Random(seed)
    start = NOW - 60 * DAY
    lists = [{"id": f"l{i}", "title": t, "sort": i} for i, t in enumerate(["Backlog", "Doing", "Review", "Done"])]
    lanes = [{"id": "s1", "title": "Second lane", "sort": 2}, {"id": "s0", "title": "First lane", "sort": 1}]
    users = [{"id": f"u{i}", "name": n} for i, n in enumerate(["Ada", "Bob", "Cy"])]
    labels = [{"id": "g0", "name": "Bug", "color": "red"}, {"id": "g1", "name": "Feature 10", "color": "green"},
              {"id": "g2", "name": "feature 9", "color": "blue"}]
    cards, activities = [], []
    t = lambda v: v
    for i in range(36):
        created = start + rng.randrange(0, 55 * DAY)
        card = {"id": f"c{i:02d}", "title": rng.choice(["Card %d" % (i % 25), "card %d" % (i % 7), "Item 10", "Item 9", "item 100"]),
                "list": rng.choice(["l0", "l1", "l2", "l3"]), "swimlane": rng.choice(["s0", "s1"]),
                "created": created, "archived": int(rng.random() < 0.15), "archivedAt": None, "start": None, "end": None,
                "due": None, "received": None, "spent": None, "overtime": int(rng.random() < 0.2),
                "assignees": rng.sample(["u0", "u1", "u2"], rng.randrange(0, 3)),
                "members": rng.sample(["u0", "u1", "u2"], rng.randrange(0, 3)),
                "labels": rng.sample(["g0", "g1", "g2"], rng.randrange(0, 3)), "vpos": 0, "vneg": 0,
                "poker": rng.choice([None, 1, 2, 3.5, 5]), "deps": [], "deleted": None,
                "sprint": rng.choice(["", "", "sp1", "sp2"]), "rank": rng.choice([None, None, 1, 2, 2.5, 10]),
                "release": rng.choice(["", "r1"]), "sort": float(rng.randrange(0, 20)),
                "estField": rng.choice([None, 3, 4.5, "8", "1.5", "x", "-2", -1]),
                "description": rng.choice(["", "Some text", "Another"])}
        for field in ("start", "end", "due", "received"):
            if rng.random() < 0.45:
                card[field] = created + rng.randrange(-5 * DAY, 25 * DAY)
        if card["archived"]:
            card["archivedAt"] = created + DAY
        if rng.random() < 0.5:
            card["spent"] = round(rng.uniform(0.5, 30), 2)
        cards.append(card)
        title0 = card["title"]
        activities.append({"type": "createCard", "card": card["id"], "list": "l0", "old": "", "hasOldList": 0,
                           "oldLane": "", "hasOldLane": 0, "member": "", "label": "", "oldValue": "", "timeKey": "",
                           "timeOld": None, "at": created})
        at = created
        for kind in rng.sample(["a-changedTitle", "moveCard", "joinMember", "unjoinMember", "addedLabel", "removedLabel",
                                "a-dueAt", "archivedCard", "restoredCard", "a-changedDescription"], 4):
            at += rng.randrange(3600000, 4 * DAY)
            if at > NOW:
                break
            a = {"type": kind, "card": card["id"], "list": card["list"], "old": "", "hasOldList": 0, "oldLane": "",
                 "hasOldLane": 0, "member": "", "label": "", "oldValue": "", "timeKey": "", "timeOld": None, "at": at}
            if kind == "a-changedTitle":
                a["oldValue"] = title0 + " old"
            elif kind == "a-changedDescription":
                a["oldValue"] = "Old description"
            elif kind == "moveCard":
                a["old"] = rng.choice(["l0", "l1"])
                a["hasOldList"] = 1
                if rng.random() < 0.5:
                    a["oldLane"] = "s0"
                    a["hasOldLane"] = 1
            elif kind in ("joinMember", "unjoinMember"):
                a["member"] = rng.choice(["u0", "u1", "u2"])
            elif kind in ("addedLabel", "removedLabel"):
                a["label"] = rng.choice(["g0", "g1", "g2"])
            elif kind == "a-dueAt":
                a["timeKey"] = rng.choice(["dueAt", "startAt"])
                a["timeOld"] = rng.choice([None, created + 3 * DAY])
            activities.append(a)
    activities.sort(key=lambda a: a["at"])
    return {"lists": lists, "swimlanes": lanes, "users": users, "labels": labels, "cards": cards,
            "activities": activities, "fields": json.dumps(FIELD_KEYS), "queries": json.dumps(QUERIES),
            "estimateField": estimate_field}


def lines(fx):
    t = lambda v: "" if v is None else str(v)
    out = []
    for l in fx["lists"]:
        out.append(f"L|{l['id']}|{l['title']}|{l['sort']}|0|0")
    for s in fx["swimlanes"]:
        out.append(f"S|{s['id']}|{s['title']}|{s['sort']}")
    for u in fx["users"]:
        out.append(f"U|{u['id']}|{u['name']}")
    for g in fx["labels"]:
        out.append(f"G|{g['id']}|{g['name']}|{g['color']}")
    if fx["estimateField"]:
        out.append("E|est")
    out.append("P|sp1|One|closed|1789000000000|1")
    out.append("P|sp2|Two|active||0")
    for c in fx["cards"]:
        est = c["estField"]
        est = "" if est is None else ("n" + str(est) if not isinstance(est, str) else "s" + est)
        out.append("|".join(["C", c["id"], c["title"], c["list"], c["swimlane"], t(c["created"]), str(c["archived"]),
                             t(c["archivedAt"]), t(c["start"]), t(c["end"]), t(c["due"]), t(c["spent"]),
                             str(c["overtime"]), ",".join(c["assignees"]), ",".join(c["labels"]), "0", "0",
                             t(c["poker"]), "", "", "b", t(c["received"]), ",".join(c["members"]), c["sprint"],
                             t(c["rank"]), c["release"], str(c["sort"]), est, c["description"]]))
    for a in fx["activities"]:
        out.append("|".join(["A", a["type"], a["card"], a["list"], a["old"], "u0", str(a["at"]), a["oldLane"], a["member"],
                             a["label"], a["oldValue"], a["timeKey"], t(a["timeOld"]), str(a["hasOldList"]),
                             str(a["hasOldLane"])]))
    return "\n".join(out) + "\n"


def number(value):
    text = f"{round(value * 100) / 100:.2f}".rstrip("0").rstrip(".")
    return "0" if text == "-0" else text


def main():
    if not (WEKAN / "models/lib/tableViewSort.js").is_file() or shutil.which("node") is None:
        print("view rows: no WeKan checkout or Node; skipped")
        return
    tmp = Path(os.environ.get("TMPDIR", tempfile.gettempdir()))
    with tempfile.TemporaryDirectory(dir=tmp) as temporary:
        work = Path(temporary)
        tool = work / "tool"
        subprocess.run(["cc", "-std=c89", "-pedantic-errors", "-Wall", "-Wextra", "-Werror",
                        str(ROOT / "tests/charts_tool.c"), str(ROOT / "models/charts.c"), str(ROOT / "models/view_data.c"),
                        str(ROOT / "models/view_rows.c"), "-lm", "-o", str(tool)], check=True)
        for seed, estimate_field in ((1, False), (2, True), (3, False)):
            fx = fixture(seed, estimate_field)
            (work / "fx.json").write_text(json.dumps(fx))
            js = subprocess.run(["node", "-e", HARNESS, str(WEKAN), str(work / "fx.json"), str(NOW)],
                                capture_output=True, text=True)
            assert js.returncode == 0, js.stderr
            want = json.loads(js.stdout)
            run = subprocess.run([str(tool), "views", str(NOW)], input=lines(fx), capture_output=True, text=True)
            assert run.returncode == 0, run.stderr
            got = {"table": {}, "calendar": [], "groups": [], "markers": [], "timeline": [], "gantt": [], "scrum": [],
                   "estimate": {}, "time-assignee": [], "time-card": []}
            for row in run.stdout.splitlines():
                p = row.split("\t")
                if p[0] == "table":
                    field = FIELD_IDS.index(int(p[1]))
                    got["table"][f"{field}|{p[2]}|{p[3]}|{p[4]}"] = p[5:]
                elif p[0] == "calendar":
                    got["calendar"].append([p[1], int(p[2]), int(p[3]), int(p[4])])
                elif p[0] == "time":
                    got["time"] = [int(p[2]), int(p[3]), int(p[4]), int(p[5])]
                elif p[0] == "time-assignee":
                    got["time-assignee"].append([p[1], p[2], int(p[3])])
                elif p[0] == "time-card":
                    got["time-card"].append(p[1])
                elif p[0] == "group":
                    got["groups"].append(p[1:])
                elif p[0] == "marker":
                    got["markers"].append(int(p[1]))
                elif p[0] == "timeline":
                    got["timeline"].append([int(p[1]), p[2], int(p[3]), p[4], p[5], int(p[6]), p[7], p[8], p[9]])
                elif p[0] == "gantt":
                    got["gantt"].append([p[1], int(p[2]), int(p[3]), int(p[4]), int(p[5])])
                elif p[0] == "scrum":
                    got["scrum"].append(p[2:])
                elif p[0] == "estimate":
                    got["estimate"][p[1]] = p[2]
            for key, order in want["table"].items():
                assert got["table"][key] == order, (seed, "table", key, got["table"][key], order)
            assert got["calendar"] == want["calendar"], (seed, "calendar", got["calendar"][:4], want["calendar"][:4])
            t = want["time"]
            assert got["time"] == [t["days"], t["hours"], t["count"], t["withTime"]], (seed, got["time"], t)
            assert got["time-assignee"] == [[g[0], number(g[1]), g[2]] for g in t["byAssignee"]], (seed, got["time-assignee"])
            assert got["time-card"] == t["byCard"], (seed, "time-card")
            assert got["groups"] == want["groups"], (seed, "groups", got["groups"], want["groups"])
            assert got["markers"] == want["markers"], (seed, "markers")
            assert got["timeline"] == [[r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[8]] for r in want["timeline"]], \
                (seed, "timeline", [g for g, w in zip(got["timeline"], want["timeline"]) if g != w][:3],
                 [w for g, w in zip(got["timeline"], want["timeline"]) if g != w][:3])
            assert got["gantt"] == want["gantt"], (seed, "gantt", got["gantt"][:3], want["gantt"][:3])
            assert got["scrum"] == want["scrum"], (seed, "scrum", got["scrum"], want["scrum"])
            assert got["estimate"] == want["estimate"], (seed, "estimate",
                {k: (v, want["estimate"][k]) for k, v in got["estimate"].items() if v != want["estimate"][k]})
    print("view rows: Table, Calendar, Time, Group by Assignee, Timeline, Gantt and Scrum the same as WeKan's on 3 boards")


if __name__ == "__main__":
    sys.exit(main())
