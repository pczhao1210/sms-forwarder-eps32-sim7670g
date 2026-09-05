const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const page = fs.readFileSync(path.join(__dirname, '../sms_forwarder_esp32s3_sim7670g/src/web_pages_full.h'), 'utf8');
for (const script of page.matchAll(/<script>([\s\S]*?)<\/script>/g)) new vm.Script(script[1]);
function getFunction(name) {
  const start = page.search(new RegExp(`        (?:async )?function ${name}\\(`));
  assert.ok(start >= 0);
  return page.slice(start, page.indexOf('\n        }', start) + '\n        }'.length);
}

async function main() {
  const elements = new Map();
  const getElementById = id => {
    if (id.endsWith('-action')) return elements.get(id) || null;
    if (!elements.has(id)) elements.set(id, {
      checked: false, value: '', name: id === 'bark-key' ? 'barkKey' : id,
      removeAttribute() {}, focus() {}, after(element) { elements.set(element.id, element); },
    });
    return elements.get(id);
  };
  const requests = [];
  const context = vm.createContext({
    console,
    document: { getElementById, createElement: () => ({ options: [], add(option) { this.options.push(option); }, setAttribute() {} }) },
    Option: class { constructor(text, value) { this.text = text; this.value = value; } },
    FormData: class extends Map { constructor() { super(); } },
    fetch: async (url, options) => {
      requests.push({ url, options });
      return { json: async () => ({ success: true, reporting: { reportHour: 0 }, sleep: { mode: 0 }, network: {} }) };
    },
    alert() {}, t: key => key, tFmt: key => key,
  });
  vm.runInContext(getFunction('saveNotificationConfig') + '\nsaveNotificationConfig();', context);
  assert.equal(requests[0].url, '/api/config/notification');
  for (const channel of ['bark', 'serverchan', 'telegram', 'dingtalk', 'feishu', 'custom']) {
    assert.equal(requests[0].options.body.get(`${channel}-enabled`), 'false');
  }
  vm.runInContext(getFunction('configureSecretInputs') + '\n' + getFunction('loadConfig') + '\nloadConfig();', context);
  await new Promise(resolve => setImmediate(resolve));
  assert.equal(getElementById('report-hour').value, 0);
  assert.equal(getElementById('sleep-mode').value, 0);
  assert.equal(getElementById('data-policy').value, 0);
  vm.runInContext('configureSecretInputs({bark: {hasKey: true, hasUrl: true}});', context);
  const input = getElementById('bark-key');
  const action = getElementById('bark-key-action');
  assert.equal(input.value, '');
  assert.equal(input.disabled, true);
  assert.equal(input.placeholder, 'secret_set');
  assert.equal(action.value, 'keep');
  assert.equal(action.name, 'barkKeyAction');
  action.value = 'replace';
  action.onchange();
  assert.equal(input.disabled, false);
  input.value = 'temporary';
  action.value = 'clear';
  action.onchange();
  assert.equal(input.value, '');
  assert.equal(input.disabled, true);
  const buttons = [{ disabled: false }, { disabled: false }];
  const dialogs = [];
  let submitted = 0;
  let polled = 0;
  context.document.querySelectorAll = () => buttons;
  context.confirm = () => true;
  context.alert = message => dialogs.push(message);
  context.setTimeout = callback => callback();
  context.fetch = async (url, options) => {
    assert.ok(buttons.every(button => button.disabled));
    if (options?.method === 'POST') {
      assert.equal(url, '/api/test/notification');
      submitted++;
      return { ok: true, json: async () => ({ id: 7, complete: false }) };
    }
    assert.equal(url, '/api/test/notification?id=7');
    polled++;
    return { ok: true, json: async () => ({ complete: polled >= 2, results: {
      bark: true, serverChan: true, telegram: false, dingtalk: true, feishu: false, custom: true,
    } }) };
  };
  vm.runInContext(getFunction('testAllNotifications') + '\n' + getFunction('testNotification'), context);
  await vm.runInContext('Promise.all([testNotification(), testAllNotifications()])', context);
  assert.equal(submitted, 1);
  assert.equal(polled, 2);
  assert.ok(buttons.every(button => !button.disabled));
  for (const channel of ['bark', 'serverChan', 'telegram', 'dingtalk', 'feishu', 'custom']) assert.ok(dialogs.at(-1).includes(channel));
  for (const failedPhase of ['submit', 'poll']) {
    context.fetch = async (url, options) => ({
      ok: failedPhase === 'poll' && options?.method === 'POST', status: 503,
      json: async () => ({ id: 7, error: 'busy' }),
    });
    await vm.runInContext('testNotification()', context);
    assert.equal(dialogs.at(-1), 'notify_test_fail');
    assert.ok(buttons.every(button => !button.disabled));
  }
  console.log('Web script syntax and configuration round-trip tests passed.');
}
main().catch(error => { console.error(error); process.exitCode = 1; });