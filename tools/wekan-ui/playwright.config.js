// @ts-check
// Captures WeKan's own board UI - layout, text, colors - as the reference the
// Nuklear desktop is compared with. Runs against a WeKan already serving on
// WEKAN_BASE_URL, reusing WeKan's Playwright fixtures from WEKAN_ROOT:
//
//   WEKAN_ROOT=../.. npx --prefix $WEKAN_ROOT/tests/playwright playwright test \
//     --config tools/wekan-ui/playwright.config.js
const path = require('path');
const { defineConfig, devices } = require(path.join(process.env.WEKAN_ROOT || '../..', 'tests/playwright/node_modules/@playwright/test'));

module.exports = defineConfig({
  testDir: __dirname,
  testMatch: /(capture|dropin)\.e2e\.js$/,
  timeout: 180_000,
  workers: 1,
  globalSetup: path.join(process.env.WEKAN_ROOT || '../..', 'tests/playwright/global-setup.js'),
  use: {
    ...devices['Desktop Chrome'],
    baseURL: process.env.WEKAN_BASE_URL || 'http://localhost:3000',
    // The desktop opens at 1024x720; compare at the same size.
    viewport: { width: 1024, height: 720 },
    deviceScaleFactor: 1,
  },
  projects: [{ name: 'chromium' }],
});
