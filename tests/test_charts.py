#!/usr/bin/env python3
"""Wena's board report charts against WeKan's own calculations.

One seeded board - lists with WIP limits, cards created, started, finished,
archived and deleted over two months, assignees, labels, votes, Planning Poker
estimates, dependencies, activities and change history - is given to both:
WeKan's models/lib/chartCalculations.js, flowAnalytics.js, chartExportRows.js
and flowAnalyticsRows.js in Node, replaying server/lib/boardChartData.js,
and Wena's models/charts.c. Every chart's table, detail table and note must
be the same, cell for cell, and the bars WeKan draws the same bars.
Skipped when there is no WeKan checkout or no Node.
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
CHARTS = ["dashboard", "burndown", "burnup", "cumulativeFlow", "controlChart", "cycleTime", "leadTime",
          "flowEfficiency", "throughputHistogram", "wipRun", "pulse", "agingWip", "blockerAnalysis",
          "monteCarlo", "processBehavior", "sizeCycleTime"]
DAY = 86400000
NOW = 1790000000000 + 13 * 3600000  # a fixed "now", mid-day UTC

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
const c = require(path.join(root, 'models/lib/chartCalculations'));
const flow = require(path.join(root, 'models/lib/flowAnalytics'));
const { chartExportRows } = require(path.join(root, 'models/lib/chartExportRows'));
const { flowDetailRows } = require(path.join(root, 'models/lib/flowAnalyticsRows'));
const en = JSON.parse(fs.readFileSync(path.join(root, 'imports/i18n/data/en.i18n.json'), 'utf8'));
const fx = JSON.parse(fs.readFileSync(fixturePath, 'utf8'));
const date = v => v == null ? undefined : new Date(v);
const cards = fx.cards.map(k => ({ _id: k.id, title: k.title, listId: k.list, boardId: k.board,
  createdAt: date(k.created), archived: !!k.archived, archivedAt: date(k.archivedAt), startAt: date(k.start),
  endAt: date(k.end), dueAt: date(k.due), spentTime: k.spent, isOvertime: !!k.overtime,
  assignees: k.assignees, labelIds: k.labels, vote: { positive: Array(k.vpos).fill('u'), negative: Array(k.vneg).fill('u') },
  poker: k.poker == null ? undefined : { estimation: k.poker },
  cardDependencies: k.deps.map(d => ({ cardId: d.id, type: d.blocks ? 'blocks' : 'is-blocked-by' })),
  deletedAt: date(k.deleted) }));
const lists = fx.lists.map(l => ({ _id: l.id, title: l.title, sort: l.sort,
  wipLimit: { enabled: !!l.wip, value: l.wipValue } }));
const activities = fx.activities.map(a => ({ activityType: a.type, cardId: a.card, listId: a.list,
  oldListId: a.old, userId: a.user, createdAt: new Date(a.at) }));
const history = fx.history.map(h => {
  const row = { entityType: 'card', entityId: h.card, userId: h.user, createdAt: new Date(h.at) };
  if (h.kind === 'position') return { ...row, group: 'position', previousContent: { listId: h.oldList },
    newContent: { listId: h.newList } };
  const deps = list => list.map(d => ({ cardId: d.id, type: d.blocks ? 'blocks' : 'is-blocked-by' }));
  const isDate = ['endAt', 'archivedAt', 'deletedAt'].includes(h.field);
  const value = (v, f) => f === 'cardDependencies' ? v : (v == null ? v : (isDate ? new Date(v) : v));
  return { ...row, group: h.field === 'cardDependencies' ? 'dependencies' : 'dates',
    previousContent: { field: h.field, value: h.field === 'cardDependencies' ? deps(h.oldDeps) : value(h.old, h.field), isDate },
    newContent: { field: h.field, value: h.field === 'cardDependencies' ? deps(h.newDeps) : value(h.new, h.field), isDate } };
});
const users = Object.fromEntries(fx.users.map(u => [u.id, u.name]));
const labelById = Object.fromEntries(fx.labels.map(l => [l.id, l.name || l.color]));
const nameOf = id => users[id] || id;
const translate = (key, fallback) => fallback;
function data(key) {
  if (flow.FLOW_CHART_KEYS.includes(key)) {
    const events = ['agingWip', 'blockerAnalysis'].includes(key) ? activities.filter(a =>
      ['createCard', 'moveCard', 'moveCardBoard', 'archivedCard', 'restoredCard'].includes(a.activityType)).map(a => ({ ...a })) : [];
    for (const row of history.filter(row => row.group === 'position'))
      events.push({ cardId: row.entityId, listId: row.newContent.listId, oldListId: row.previousContent.listId,
        createdAt: row.createdAt, activityType: 'moveCard' });
    return flow.computeFlowAnalytics(key, cards, lists, events, [], {}, new Date(), history);
  }
  const first = cards.reduce((min, card) => (!min || card.createdAt < min ? card.createdAt : min), null);
  const from = first || new Date(), to = new Date();
  if (key === 'pulse') {
    const t = new Date(); const f = new Date(t); f.setUTCDate(f.getUTCDate() - 29);
    return { series: c.computeActivityPulse(activities.filter(a => a.createdAt >= f && a.createdAt <= t), f, t, 'day') };
  }
  if (key === 'cumulativeFlow' || key === 'wipRun') {
    const ev = activities.filter(a => ['createCard', 'moveCard', 'archivedCard', 'restoredCard'].includes(a.activityType))
      .sort((a, b) => a.createdAt - b.createdAt);
    if (key === 'cumulativeFlow') return { lists: lists.map(l => ({ _id: l._id, title: l.title })),
      series: c.computeCumulativeFlow(lists, ev, from, to) };
    const ids = lists.length > 2 ? lists.slice(1, -1).map(l => l._id) : lists.map(l => l._id);
    const limit = lists.length > 2 ? lists.slice(1, -1).reduce((s, l) => (l.wipLimit && l.wipLimit.enabled) ? s + (l.wipLimit.value || 0) : s, 0) || null : null;
    return { series: c.computeWipRun(ids, ev, from, to, limit) };
  }
  if (key === 'controlChart') return { points: c.computeControlChart(cards) };
  if (key === 'leadTime' || key === 'cycleTime') return { points: c.computeLeadCycleTime(cards) };
  if (key === 'burndown') return { series: c.computeBurndown(cards, from, to) };
  if (key === 'burnup') return { series: c.computeBurnup(cards, from, to) };
  if (key === 'throughputHistogram') { const series = c.computeThroughput(cards, 'week');
    return { series, forecast: c.computeCompletionForecast(cards, series, 7, 4) }; }
  if (key === 'flowEfficiency') return { points: c.computeFlowEfficiency(cards) };
  if (key === 'dashboard') return {
    byAssignee: c.computeDashboardGroups(cards, k => (k.assignees || []).map(id => ({ key: id, label: nameOf(id) })), c.NO_ASSIGNEE_GROUP),
    byLabel: c.computeDashboardGroups(cards, k => (k.labelIds || []).map(id => ({ key: id, label: labelById[id] || id })), c.NO_LABEL_GROUP),
    byList: c.computeDashboardGroups(cards, k => [{ key: k.listId, label: (lists.find(l => l._id === k.listId) || {}).title || k.listId }]) };
}
const pad = n => String(n).padStart(2, '0');
const cell = v => v instanceof RealDate ? `${v.toISOString().slice(0, 10)} ${pad(v.getUTCHours())}:${pad(v.getUTCMinutes())}` : String(v);
const fill = (key, values) => (en[key] || key).replace(/__(\w+)__/g, (m, k) => values[k]);
function bars(key, d) {
  const last = (series, v, l) => series.slice(-24).map(r => [r[l], Math.round((Number(r[v]) || 0) * 100) / 100]);
  if (key === 'burndown') return last(d.series, 'remaining', 'day');
  if (key === 'burnup') return last(d.series, 'completed', 'day');
  if (key === 'wipRun' || key === 'pulse') return last(d.series, 'count', 'day');
  if (key === 'throughputHistogram') return last(d.series, 'count', 'bucket');
  if (key === 'cumulativeFlow') return last(d.lists.map(l => ({ list: l.title,
    day: (d.series[d.series.length - 1] || { counts: {} }).counts[l._id] || 0 })), 'day', 'list');
  if (['controlChart', 'leadTime', 'cycleTime'].includes(key)) return last(d.points, 'cycleDays' in (d.points[0] || {}) ? 'cycleDays' : 'leadDays', 'title');
  if (key === 'flowEfficiency') return last(d.points, 'efficiency', 'title');
  if (key === 'dashboard') return last(d.byAssignee, 'count', 'label').map(r => [r[0] === '__no_assignee__' ? 'No assignee' : r[0], r[1]]);
  if (key === 'agingWip') return d.points.filter(p => p.ageDays !== null).slice(-200).map(p => [`${p.title} (${p.list})`, Math.round(p.ageDays * 100) / 100]);
  return null;
}
const out = {};
for (const key of JSON.parse(fx.charts)) {
  const d = data(key);
  const table = chartExportRows(key, d, translate);
  const detail = flowDetailRows(key, d, translate);
  let rows = table.rows.map(r => r.map(cell));
  if (key === 'monteCarlo') rows = rows.map(r => [`${Math.round(parseFloat(r[0]))}%`, ...r.slice(1)]);
  const dashboardLabel = v => v === '__no_assignee__' ? 'No assignee' : v === '__no_label__' ? 'No label' : v;
  if (key === 'dashboard') rows = rows.map(r => [dashboardLabel(r[0]), r[1]]);
  let note = '';
  if (key === 'throughputHistogram') {
    const f = d.forecast;
    note = f.remaining === 0 ? fill('chart-forecast-none-remaining', {}) : !f.projectedDate
      ? fill('chart-forecast-no-velocity', { remaining: f.remaining })
      : fill('chart-forecast-projected', { remaining: f.remaining, average: f.averagePerBucket, date: f.projectedDate });
  }
  out[key] = { headers: table.headers.map(String), rows, detailHeaders: detail.headers.map(String),
    detail: detail.rows.map(r => r.map(cell)), note, bars: bars(key, d) };
}
process.stdout.write(JSON.stringify(out));
"""


def fixture(seed):
    rng = random.Random(seed)
    start = NOW - 60 * DAY
    lists = [{"id": f"l{i}", "title": t, "sort": i, "wip": i in (1, 2), "wipValue": 3 + i}
             for i, t in enumerate(["Backlog", "Doing", "Review", "Done"])]
    users = [{"id": f"u{i}", "name": n} for i, n in enumerate(["Ada", "Bob", "Cy"])]
    labels = [{"id": "g0", "name": "Bug", "color": "red"}, {"id": "g1", "name": "", "color": "green"}]
    cards, activities, history = [], [], []
    for i in range(40):
        created = start + rng.randrange(0, 55 * DAY)
        card = {"id": f"c{i:02d}", "title": f"Card {i % 30}", "list": "l0", "swimlane": "s", "board": "b",
                "created": created, "archived": 0, "archivedAt": None, "start": None, "end": None, "due": None,
                "spent": None, "overtime": 0, "assignees": [], "labels": [], "vpos": rng.randrange(0, 3),
                "vneg": rng.randrange(0, 2), "poker": None, "deps": [], "deleted": None}
        activities.append({"type": "createCard", "card": card["id"], "list": "l0", "old": "", "user": "u0", "at": created})
        at = created
        for target in ("l1", "l2", "l3"):
            if rng.random() < 0.55:
                break
            at += rng.randrange(3600000, 6 * DAY)
            if at > NOW:
                break
            if target == "l1" and rng.random() < 0.6:
                card["start"] = at
            if rng.random() < 0.4:
                history.append({"kind": "position", "card": card["id"], "user": "u1", "oldList": card["list"],
                                "newList": target, "at": at})
            else:
                activities.append({"type": "moveCard", "card": card["id"], "list": target, "old": card["list"],
                                   "user": "u1", "at": at})
            card["list"] = target
        if card["list"] == "l3" and rng.random() < 0.7:
            card["end"] = min(NOW - 3600000, at + rng.randrange(0, DAY))
        if rng.random() < 0.15:
            at2 = min(NOW - 1000, at + rng.randrange(0, 3 * DAY))
            card["archived"] = 1
            card["archivedAt"] = at2
            activities.append({"type": "archivedCard", "card": card["id"], "list": card["list"], "old": "", "user": "u2", "at": at2})
        if rng.random() < 0.4:
            card["due"] = created + rng.randrange(DAY, 30 * DAY)
        if rng.random() < 0.5:
            card["spent"] = round(rng.uniform(0.5, 30), 2)
        card["assignees"] = rng.sample(["u0", "u1", "u2"], rng.randrange(0, 3))
        card["labels"] = rng.sample(["g0", "g1"], rng.randrange(0, 3))
        if rng.random() < 0.5:
            card["poker"] = rng.choice([1, 2, 3, 5, 8])
        cards.append(card)
    # Dependencies, and their history.
    for i in range(0, 12, 3):
        a, b = cards[i], cards[i + 1]
        a["deps"] = [{"id": b["id"], "blocks": True}]
        history.append({"kind": "field", "card": a["id"], "user": "u0", "field": "cardDependencies",
                        "oldDeps": [], "newDeps": a["deps"], "at": max(a["created"], b["created"]) + DAY})
    activities.sort(key=lambda a: a["at"])
    history.sort(key=lambda h: h["at"])
    for a in activities[-30:]:
        a["at"] = min(a["at"], NOW - 1000)
    return {"lists": lists, "users": users, "labels": labels, "cards": cards, "activities": activities,
            "history": history, "charts": json.dumps(CHARTS)}


def lines(fx):
    out = []
    t = lambda v: "" if v is None else str(v)
    for l in fx["lists"]:
        out.append(f"L|{l['id']}|{l['title']}|{l['sort']}|{int(l['wip'])}|{l['wipValue']}")
    for u in fx["users"]:
        out.append(f"U|{u['id']}|{u['name']}")
    for g in fx["labels"]:
        out.append(f"G|{g['id']}|{g['name']}|{g['color']}")
    for c in fx["cards"]:
        deps = ";".join(f"{d['id']}:{'b' if d['blocks'] else 'i'}" for d in c["deps"])
        out.append("|".join(["C", c["id"], c["title"], c["list"], c["swimlane"], t(c["created"]), str(c["archived"]),
                             t(c["archivedAt"]), t(c["start"]), t(c["end"]), t(c["due"]), t(c["spent"]),
                             str(c["overtime"]), ",".join(c["assignees"]), ",".join(c["labels"]), str(c["vpos"]),
                             str(c["vneg"]), t(c["poker"]), deps, t(c["deleted"]), c["board"]]))
    for a in fx["activities"]:
        out.append(f"A|{a['type']}|{a['card']}|{a['list']}|{a['old']}|{a['user']}|{a['at']}")
    for h in fx["history"]:
        deps = lambda ds: ";".join(f"{d['id']}:{'b' if d['blocks'] else 'i'}" for d in ds)
        if h["kind"] == "position":
            out.append(f"H|position|{h['card']}|{h['user']}||{h['oldList']}|{h['newList']}||||||{h['at']}|x|x")
        else:
            out.append(f"H|field|{h['card']}|{h['user']}|{h['field']}||||||{deps(h.get('oldDeps', []))}|"
                       f"{deps(h.get('newDeps', []))}|{h['at']}|x|x")
    return "\n".join(out) + "\n"


def parse(output):
    result = {"table": [], "detail": [], "note": "", "plot": []}
    for row in output.splitlines():
        parts = row.split("\t")
        if parts[0] in ("table", "detail"):
            result[parts[0]].append(parts[1:])
        elif parts[0] == "note":
            result["note"] = parts[1]
        elif parts[0] == "plot":
            result["plot"].append([parts[1], parts[2]])
    return result


def number(value):
    text = f"{round(value * 100) / 100:.2f}".rstrip("0").rstrip(".")
    return "0" if text == "-0" else text


def main():
    if not (WEKAN / "models/lib/chartCalculations.js").is_file() or shutil.which("node") is None:
        print("charts: no WeKan checkout or Node; skipped")
        return
    tmp = Path(os.environ.get("TMPDIR", tempfile.gettempdir()))
    with tempfile.TemporaryDirectory(dir=tmp) as temporary:
        work = Path(temporary)
        tool = work / "charts_tool"
        subprocess.run(["cc", "-std=c89", "-pedantic-errors", "-Wall", "-Wextra", "-Werror",
                        str(ROOT / "tests/charts_tool.c"), str(ROOT / "models/charts.c"),
                        str(ROOT / "models/view_data.c"), str(ROOT / "models/view_rows.c"), "-lm", "-o", str(tool)], check=True)
        for seed in (1, 2, 3):
            fx = fixture(seed)
            (work / "fixture.json").write_text(json.dumps(fx))
            js = subprocess.run(["node", "-e", HARNESS, str(WEKAN), str(work / "fixture.json"), str(NOW)],
                                capture_output=True, text=True)
            assert js.returncode == 0, js.stderr
            expected = json.loads(js.stdout)
            for chart in CHARTS:
                run = subprocess.run([str(tool), chart, str(NOW)], input=lines(fx), capture_output=True, text=True)
                assert run.returncode == 0, (chart, run.stderr)
                got = parse(run.stdout)
                want = expected[chart]
                assert got["table"][0] == want["headers"], (seed, chart, got["table"][0], want["headers"])
                for index, (g, w) in enumerate(zip(got["table"][1:], want["rows"])):
                    assert g == [str(x) for x in w], (seed, chart, index, g, w)
                assert len(got["table"]) - 1 == len(want["rows"]), (seed, chart, len(got["table"]) - 1, len(want["rows"]))
                if want["detailHeaders"]:
                    assert got["detail"][0] == want["detailHeaders"], (seed, chart, got["detail"][0])
                    assert got["detail"][1:] == want["detail"], (seed, chart, got["detail"][1:6], want["detail"][:5])
                assert got["note"] == want["note"], (seed, chart, got["note"], want["note"])
                if want["bars"] is not None:
                    bars = [[label, number(value)] for label, value in want["bars"]]
                    assert got["plot"] == bars, (seed, chart, got["plot"][:5], bars[:5])
    print("charts: %d charts on 3 boards the same as WeKan's calculations" % len(CHARTS))


if __name__ == "__main__":
    sys.exit(main())
