'use strict';
// The WeKan board as a user sees it - each state's screenshot, and every
// visible control with its region, text, tooltip, icon, place and colors -
// written to WENA_CAPTURE_DIR as STATE.png and STATE.json. The Nuklear
// desktop's layout and colors are compared with these (docs/wekan-ui-parity.md).
const fs = require('fs');
const path = require('path');
const root = process.env.WEKAN_ROOT || path.join(__dirname, '../../../..');
const { test, expect } = require(path.join(root, 'tests/playwright/fixtures'));
const BoardPage = require(path.join(root, 'tests/playwright/pages/BoardPage'));

const OUT = process.env.WENA_CAPTURE_DIR || path.join(__dirname, 'capture');

// Areas of the page, nearest first: a control belongs to the first that contains it.
const REGIONS = [
  ['popup', '.js-pop-over'],
  ['all-boards-menu', '.boards-left-menu'],
  ['board-tile', '.js-board, .js-add-board'],
  ['all-boards', '.boards-right-grid, .board-list'],
  ['card-details', '.js-card-details'],
  ['sidebar', '.sidebar.is-open, .board-sidebar.is-open'],
  ['composer', '.js-inlined-form, .js-card-composer, .list-composer'],
  ['minicard', '.js-minicard'],
  ['list-header', '.list-header'],
  ['list', '.js-list'],
  ['swimlane-header', '.swimlane-header-wrap, .swimlane-header'],
  ['swimlane', '.swimlane'],
  ['board-header', '.board-header, .board-header-btns'],
  ['header', '#header, .header-bar, header'],
  ['board', '.board-canvas, .board-wrapper'],
];

async function closePopup(page) {
  const close = page.locator('.js-pop-over .js-close-pop-over').first();
  if (await close.isVisible().catch(() => false)) await close.click();
  await page.locator('.js-pop-over').waitFor({ state: 'hidden', timeout: 5_000 }).catch(() => {});
}

async function snapshot(page, name) {
  fs.mkdirSync(OUT, { recursive: true });
  await page.waitForTimeout(600);
  await page.screenshot({ path: path.join(OUT, `${name}.png`) });
  const data = await page.evaluate((regions) => {
    const visible = (el) => {
      const r = el.getBoundingClientRect();
      const s = getComputedStyle(el);
      return r.width > 0 && r.height > 0 && s.visibility !== 'hidden' && s.display !== 'none' &&
        r.bottom > 0 && r.right > 0 && r.top < innerHeight && r.left < innerWidth;
    };
    const region = (el) => {
      for (const [name, selector] of regions) if (el.closest(selector)) return name;
      return 'page';
    };
    const icon = (el) => {
      for (const node of [el, ...el.querySelectorAll('i, .fa, [class*="icon"]')].slice(0, 3)) {
        const before = getComputedStyle(node, '::before').content;
        if (before && before !== 'none' && before !== 'normal' && before !== '""') return before;
      }
      return null;
    };
    const colors = (el) => {
      const s = getComputedStyle(el);
      return { background: s.backgroundColor, color: s.color, border: s.borderTopColor,
               borderWidth: s.borderTopWidth, radius: s.borderTopLeftRadius,
               font: `${s.fontWeight} ${s.fontSize} ${s.fontFamily.split(',')[0]}` };
    };
    const selector = 'a, button, input, textarea, select, [role="button"], [class*="js-"], .fa, i';
    const controls = [];
    const seen = new Set();
    for (const el of document.querySelectorAll(selector)) {
      if (!visible(el) || seen.has(el)) continue;
      // The outermost control of a nested group speaks for it.
      if (el.parentElement && el.parentElement.closest(selector) &&
          seen.has(el.parentElement.closest(selector))) continue;
      seen.add(el);
      const r = el.getBoundingClientRect();
      controls.push({
        region: region(el),
        tag: el.tagName.toLowerCase(),
        classes: [...el.classList].filter(c => c.startsWith('js-') || /header|card|list|swimlane|btn|icon|title|badge|label|menu|plus|handle|drag/.test(c)).slice(0, 8),
        text: (el.innerText || el.value || '').trim().replace(/\s+/g, ' ').slice(0, 80),
        title: el.getAttribute('title') || el.getAttribute('aria-label') || null,
        icon: icon(el),
        draggable: !!(el.closest('.ui-sortable-handle, .handle, [draggable="true"], .js-minicard, .js-list-handle, .js-swimlane-header-handle')),
        rect: [Math.round(r.left), Math.round(r.top), Math.round(r.width), Math.round(r.height)],
        style: colors(el),
      });
    }
    // The surfaces themselves: the colors a native theme has to reproduce.
    const surfaces = {};
    for (const [name, selector] of [
      ['body', 'body'], ['header', '#header'], ['header-quick-access', '#header-quick-access'],
      ['board-header', '.board-header'], ['board-canvas', '.board-canvas'], ['board-wrapper', '.board-wrapper'],
      ['swimlane', '.swimlane'], ['swimlane-header', '.swimlane-header-wrap'],
      ['list', '.js-list:not(.js-list-composer)'], ['list-header', '.list-header'], ['list-body', '.list-body'],
      ['minicard', '.minicard'], ['minicard-title', '.minicard-title'], ['open-composer', '.open-list-composer, .js-open-inlined-form'],
      ['card-details', '.js-card-details'], ['card-details-title', '.card-details-title'],
      ['card-details-item-title', '.card-details-item-title'],
      ['popup', '.js-pop-over'], ['popup-header', '.js-pop-over .header'], ['popup-item', '.js-pop-over li a'],
      ['sidebar', '.sidebar.is-open, .board-sidebar.is-open'], ['button-primary', '.button.primary, button.primary, .js-submit, button[type=submit]'],
      ['input', 'textarea, input[type=text]'],
      ['all-boards-menu', '.boards-left-menu'], ['all-boards-menu-item', '.boards-left-menu .menu-item a'],
      ['all-boards-menu-active', '.boards-left-menu .menu-item.active a'],
      ['all-boards-menu-count', '.boards-left-menu .menu-count'],
      ['board-tile', '.js-board .board-list-item'], ['board-tile-title', '.js-board .board-list-item-name'],
      ['add-board-tile', '.js-add-board .board-list-item'], ['pane-title', '.boards-right-grid h1, .pane-title, .boards-right-grid .title']]) {
      const el = [...document.querySelectorAll(selector)].find(visible);
      if (!el) continue;
      const r = el.getBoundingClientRect();
      surfaces[name] = { ...colors(el), rect: [Math.round(r.left), Math.round(r.top), Math.round(r.width), Math.round(r.height)] };
    }
    return { url: location.pathname, viewport: [innerWidth, innerHeight], surfaces, controls };
  }, REGIONS);
  fs.writeFileSync(path.join(OUT, `${name}.json`), JSON.stringify(data, null, 1));
  return data;
}

test('capture the WeKan board UI for the native desktop', async ({ boardPage: page, board }) => {
  const boards = new BoardPage(page);
  const [first] = board.listIds;
  await page.locator('.js-minicard').first().waitFor();
  await snapshot(page, '01-board');

  // All Boards, WeKan's first page; then back to the board.
  const boardUrl = page.url();
  await page.goto('/');
  await page.locator('.js-board, .js-add-board').first().waitFor();
  await snapshot(page, '00-all-boards');
  await page.goto(boardUrl);
  await page.locator('.js-minicard').first().waitFor();

  await boards.openListMenu(first);
  await snapshot(page, '02-list-menu');
  await closePopup(page);

  await boards.openAddCardTop(first);
  await snapshot(page, '03-add-card');
  await boards.closeComposers(first);

  const swimlaneMenu = page.locator('.js-open-swimlane-menu').first();
  if (await swimlaneMenu.isVisible().catch(() => false)) {
    await swimlaneMenu.click();
    await page.locator('.js-pop-over').waitFor();
    await snapshot(page, '04-swimlane-menu');
    await closePopup(page);
  }

  const addList = page.locator('.js-open-list-composer, .open-list-composer, .js-list-composer').first();
  if (await addList.isVisible().catch(() => false)) {
    await addList.click();
    await snapshot(page, '05-add-list');
    await closePopup(page);
  }

  await boards.openSidebar();
  await snapshot(page, '06-sidebar');
  await page.locator('.js-toggle-page-sidebar').click();

  await boards.clickCard(first, 'Alpha Card');
  await snapshot(page, '07-card-details');

  const cardMenu = page.locator('.js-card-details .js-open-card-details-menu').first();
  if (await cardMenu.isVisible().catch(() => false)) {
    await cardMenu.click();
    await page.locator('.js-pop-over').waitFor();
    await snapshot(page, '08-card-menu');
    await closePopup(page);
  }
  expect(fs.existsSync(path.join(OUT, '01-board.json'))).toBe(true);
});
