'use strict';
// WeKan itself on the wekan-files Wena wrote: the board Wena made is on
// WeKan's All Boards, and its list, card, description and checklist are on
// the board. Run by tools/wekan-ui/dropin.sh.
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
});
