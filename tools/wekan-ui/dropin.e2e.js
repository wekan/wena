'use strict';
// WeKan itself on the wekan-files Wena wrote: the board Wena made is on
// WeKan's All Boards, and its list, card, description and checklist are on
// the board. Then the other way: a board, list and card made in WeKan's own
// UI, whose board id goes to WENA_CAPTURE_DIR/wekan-board.txt for Wena to
// open. Run by tools/wekan-ui/dropin.sh.
const fs = require('fs');
const path = require('path');
const root = process.env.WEKAN_ROOT || path.join(__dirname, '../../../..');
const { test, expect } = require(path.join(root, 'tests/playwright/node_modules/@playwright/test'));
const { loginWithToken } = require(path.join(root, 'tests/playwright/helpers/auth'));
const BoardPage = require(path.join(root, 'tests/playwright/pages/BoardPage'));

const OUT = process.env.WENA_CAPTURE_DIR || path.join(__dirname, 'capture');

test('WeKan opens the board, list and card Wena wrote', async ({ page }) => {
  const user = JSON.parse(process.env.WENA_DROPIN_USER);
  fs.mkdirSync(OUT, { recursive: true });
  await loginWithToken(page, user.id, user.token);
  await page.goto('/');
  const tile = page.locator('.js-board', { hasText: 'My board' }).first();
  await tile.waitFor({ timeout: 60_000 });
  await page.screenshot({ path: path.join(OUT, 'dropin-all-boards.png') });
  await tile.locator('.js-open-board').first().click();
  const card = page.locator('.js-minicard', { hasText: 'Made in Wena' }).first();
  await card.waitFor({ timeout: 60_000 });
  await expect(page.locator('.list-header', { hasText: 'To Do' }).first()).toBeVisible();
  await page.screenshot({ path: path.join(OUT, 'dropin-board.png') });
  // WeKan's own way to open a card's details (its title edits it).
  await new BoardPage(page).clickCard('dropinList', 'Made in Wena');
  await expect(page.locator('.js-card-details').first()).toContainText('Written by Wena into wekan.sqlite');
  await expect(page.locator('.js-card-details').first()).toContainText('Open in WeKan');
  await page.screenshot({ path: path.join(OUT, 'dropin-card.png') });
  // WeKan's UI makes a board, a list and a card; Wena opens them next.
  // From All Boards, whose header has WeKan's Add Board (+).
  await page.goto('/');
  await page.locator('.js-board', { hasText: 'My board' }).first().waitFor({ timeout: 60_000 });
  await page.locator('.js-create-board').first().click();
  const popup = page.locator('.js-pop-over');
  await popup.locator('.js-new-board-title').fill('Made in WeKan');
  await popup.locator('input[type="submit"]').click();
  await page.waitForURL(/\/b\/[^/]+\/made-in-wekan/, { timeout: 60_000 });
  const board = page.url().match(/\/b\/([^/]+)\//)[1];
  await page.locator('.js-open-empty-add-list').click();
  const composer = page.locator('.js-add-list-inline-form');
  await composer.locator('.list-name-input').fill('WeKan list');
  await composer.getByRole('button', { name: 'Save' }).click();
  const list = page.locator('.js-list:not(.js-list-composer)', { hasText: 'WeKan list' }).first();
  await list.waitFor({ timeout: 30_000 });
  const listId = (await list.getAttribute('id')).replace(/^js-list-/, '');
  const boardPage = new BoardPage(page);
  await boardPage.openAddCardTop(listId);
  await boardPage.submitNewCard(listId, 'WeKan card');
  await page.screenshot({ path: path.join(OUT, 'dropin-wekan-board.png') });
  fs.writeFileSync(path.join(OUT, 'wekan-board.txt'), board);
});
