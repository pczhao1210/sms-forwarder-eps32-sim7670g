const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { chromium } = require('playwright');

const source = fs.readFileSync(path.join(__dirname, '../sms_forwarder_esp32s3_sim7670g/src/web_pages_full.h'), 'utf8');
const html = source.match(/R"rawliteral\(([\s\S]*?)\)rawliteral";/)[1];
const config = {
  lang: 'en', wifi: { ssid: 'Test Network', hasPassword: true },
  bark: { enabled: true, hasKey: true, hasUrl: true },
  serverChan: { enabled: true, hasKey: true, hasUrl: true },
  telegram: { enabled: true, hasToken: true, hasChatId: true, hasUrl: true },
  dingtalk: { enabled: true, hasWebhook: true }, feishu: { enabled: true, hasWebhook: true },
  custom: { enabled: true, hasKey: true, hasUrl: true },
  reporting: { reportHour: 0 }, sleep: { mode: 0 }, network: { dataPolicy: 0, hasApnUser: true, hasApnPass: true },
  tls: { privateCaHost: '' }, webAuth: { enabled: true, username: 'admin', hasPassword: true },
};

async function main() {
  const browser = await chromium.launch({ headless: true });
  const artifacts = fs.mkdtempSync(path.join(os.tmpdir(), 'sms-browser-test-'));
  try {
    for (const viewport of [{ width: 1440, height: 1000 }, { width: 390, height: 844 }]) {
      const context = await browser.newContext({ viewport, isMobile: viewport.width < 500, hasTouch: viewport.width < 500 });
      const page = await context.newPage();
      const errors = [];
      const dialogs = [];
      const saves = [];
      let resultPolls = 0;
      let rejectTest = false;
      page.on('pageerror', error => errors.push(error.message));
      page.on('dialog', async dialog => { dialogs.push(dialog.message()); await dialog.accept(); });
      await page.route('http://sms-forwarder.test/**', async route => {
        const request = route.request();
        const url = new URL(request.url());
        let status = 200;
        let body = { success: true };
        if (url.pathname === '/') {
          await route.fulfill({ contentType: 'text/html', body: html });
          return;
        }
        if (url.pathname === '/api/config') body = config;
        if (url.pathname === '/api/config/notification' && request.method() === 'POST') {
          const decoded = new Request(request.url(), { method: 'POST', headers: request.headers(), body: request.postDataBuffer() });
          saves.push(Object.fromEntries(await decoded.formData()));
        }
        if (url.pathname === '/api/test/notification') {
          if (rejectTest) {
            status = 503;
            body = { error: 'notification_test_busy' };
          } else if (request.method() === 'POST') {
            status = 202;
            resultPolls = 0;
            body = { id: 1, complete: false };
          } else {
            resultPolls++;
            body = { id: 1, complete: resultPolls >= 2, results: { bark: true, serverChan: true, telegram: false, dingtalk: true, feishu: false, custom: true }, total: 6, success: 4 };
          }
        }
        await route.fulfill({ status, contentType: 'application/json', body: JSON.stringify(body) });
      });
      await page.goto('http://sms-forwarder.test/#config');
      await page.waitForFunction(() => document.getElementById('bark-key-action')?.options[0]?.textContent === 'Keep unchanged');
      assert.equal(await page.locator('#bark-key').inputValue(), '');
      assert.equal(await page.locator('#bark-key').isDisabled(), true);
      assert.equal(await page.locator('#report-hour').inputValue(), '0');
      assert.equal(await page.locator('#sleep-mode').inputValue(), '0');
      assert.equal(await page.locator('#data-policy').inputValue(), '0');
      for (const channel of ['bark', 'serverchan', 'telegram', 'dingtalk', 'feishu', 'custom']) {
        await page.locator(`#${channel}-enabled`).uncheck();
      }
      await page.locator('#bark-key-action').selectOption('replace');
      await page.locator('#bark-key').fill('browser-test-token');
      await Promise.all([
        page.waitForResponse(response => response.url().endsWith('/api/config/notification')),
        page.locator('#notificationForm button[type="submit"]').click(),
      ]);
      assert.equal(saves[0].barkKey, 'browser-test-token');
      assert.equal(saves[0].barkKeyAction, 'replace');
      assert.equal(saves[0].serverChanKeyAction, 'keep');
      assert.equal(saves[0].serverChanKey, undefined);
      for (const channel of ['bark', 'serverchan', 'telegram', 'dingtalk', 'feishu', 'custom']) assert.equal(saves[0][`${channel}-enabled`], 'false');
      await page.locator('#bark-key-action').selectOption('clear');
      assert.equal(await page.locator('#bark-key').isDisabled(), true);
      assert.equal(await page.locator('#bark-key').inputValue(), '');
      await Promise.all([
        page.waitForResponse(response => response.url().endsWith('/api/config/notification')),
        page.locator('#notificationForm button[type="submit"]').click(),
      ]);
      assert.equal(saves[1].barkKeyAction, 'clear');
      assert.equal(saves[1].barkKey, undefined);
      const testButton = page.locator('[onclick="testAllNotifications()"]');
      await testButton.click();
      await page.waitForFunction(() => !document.querySelector('[onclick="testAllNotifications()"]').disabled);
      assert.ok(resultPolls >= 2);
      const result = dialogs.at(-1);
      for (const channel of ['bark', 'serverChan', 'telegram', 'dingtalk', 'feishu', 'custom']) assert.ok(result.includes(channel));
      await page.evaluate(() => testNotification());
      assert.ok(resultPolls >= 2);
      assert.ok(dialogs.at(-1).includes('custom'));
      rejectTest = true;
      await testButton.click();
      await page.waitForFunction(() => !document.querySelector('[onclick="testAllNotifications()"]').disabled);
      assert.ok(dialogs.at(-1).includes('notification_test_busy'));
      assert.equal(await page.locator('[onclick="testNotification()"]').isDisabled(), false);
      const bounds = await page.evaluate(() => ({ width: innerWidth, scroll: document.documentElement.scrollWidth }));
      assert.equal(bounds.width, viewport.width);
      assert.ok(bounds.scroll <= bounds.width + 1, JSON.stringify(bounds));
      const overflows = await page.locator('#notificationForm input, #notificationForm select, #notificationForm button').evaluateAll(elements => elements.filter(element => {
        const rect = element.getBoundingClientRect();
        return rect.left < 0 || rect.right > innerWidth + 1 || element.scrollWidth > element.clientWidth + 2;
      }).map(element => element.id || element.textContent));
      assert.deepEqual(overflows, []);
      await page.locator('#notificationForm').screenshot({ path: path.join(artifacts, `notification-${viewport.width}.png`) });
      assert.deepEqual(errors, []);
      await context.close();
    }
    console.log(`Desktop/mobile browser regressions passed. Screenshots: ${artifacts}`);
  } finally {
    await browser.close();
  }
}
main().catch(error => { console.error(error); process.exitCode = 1; });